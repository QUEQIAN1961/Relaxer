#include "playback.h"
#include "importer.h"
#include "lyric-text.h"
#include <QAudioDevice>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMediaDevices>
#include <QSaveFile>
#include <QUrl>
#include <algorithm>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif
namespace relaxer {
Playback::Playback(Library *l, QObject *p, bool decodeOnly)
    : QObject(p), m_library(l), m_output(this), m_player(this), m_decodeOnly(decodeOnly) {
    // Headless self-tests must not initialize an arbitrary physical audio driver.
    // Real application launches do not set this variable and keep normal audio.
    if (!qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS") &&
        !QMediaDevices::defaultAudioOutput().isNull())
        m_player.setAudioOutput(&m_output);
    m_output.setVolume(decodeOnly ? 0 : float(l->setting("volume", 65).toInt()) / 100);
    m_player.setLoops(1);
    m_queue.mode = static_cast<Mode>(qBound(0, l->setting("mode", 1).toInt(), 3));
    m_elapsed.start();
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        sample();
        if (m_midi && m_midiPlaying) {
            const auto pos = position();
            const auto dur = duration();
            emit positionChanged(pos, dur);
            if (dur > 0 && pos + 120 >= dur) {
                m_midiPlaying = false;
                finish();
                auto id = m_queue.next(false);
                if (id)
                    load(id, true);
                else {
                    stopMidi();
                    emit stateChanged();
                    saveQueue();
                }
            }
            return;
        }
        emit positionChanged(position(), duration());
    });
    m_timer.start();
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this](auto state) {
        if (m_wasPlaying && state != QMediaPlayer::PlayingState)
            sample(false, true);
        m_wasPlaying = state == QMediaPlayer::PlayingState;
        m_lastPosition = m_player.position();
        m_lastWall = m_elapsed.elapsed();
        emit stateChanged();
    });
    connect(&m_player, &QMediaPlayer::durationChanged, this, [this](qint64 d) {
        if (d > 0) {
            m_session.duration = d;
            m_library->saveSession(m_session, 0, m_threshold);
        }
        emit positionChanged(m_player.position(), d);
    });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](auto s) {
        if (s == QMediaPlayer::LoadedMedia || s == QMediaPlayer::BufferedMedia) {
            if (m_loading) {
                m_loading = false;
                if (m_restorePosition >= 0) {
                    auto pos = qBound<qint64>(0, m_restorePosition, qMax<qint64>(0, m_player.duration() - 1));
                    m_player.setPosition(pos);
                    m_restorePosition = -1;
                }
                m_lastPosition = m_player.position();
                m_lastWall = m_elapsed.elapsed();
                if (m_autoStart)
                    m_player.play();
            }
        } else if (s == QMediaPlayer::EndOfMedia) {
            finish();
            auto id = m_queue.next(false);
            if (id)
                load(id, true);
            else {
                haltTransport();
                emit stateChanged();
                saveQueue();
            }
        }
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](auto, const QString &e) { onError(e); });
}
Playback::~Playback() { shutdown(); }
bool Playback::shutdown() {
    if (m_shutdown || m_shutdownInProgress) return m_shutdownSaved;
    m_shutdownInProgress = true;
    m_timer.stop();
    m_shutdownSaved = save();
    m_shutdown = true;
    ++m_playGeneration;
    m_autoStart = false; m_loading = false; m_restorePosition = -1;
    m_player.disconnect(this);
    haltTransport();
    m_player.setSource({});
    m_player.setAudioOutput(nullptr);
    return m_shutdownSaved;
}
bool Playback::sample(bool finishing, bool flush) {
    if (m_shutdown) return true;
    return sampleAt(m_elapsed.elapsed(), position(), playing(), finishing, flush);
}
bool Playback::sampleAt(qint64 now, qint64 position, bool isPlaying, bool finishing, bool flush) {
    if (!m_session.id) return true;
    bool ok = true;
    auto elapsed = qMax<qint64>(0, now - m_lastWall);
    qint64 delta = position - m_lastPosition;
    // Account for advanced media samples only, bounded by elapsed wall time.
    // Explicit seeks reset the baseline and disqualify the complete round.
    if ((isPlaying || finishing || flush) && !m_loading && delta > 0)
        delta = qMin(delta, elapsed + 80);
    else
        delta = 0;
    if (delta > 0 || finishing)
        ok = m_library->saveSession(m_session, delta, m_threshold, finishing);
    // Some backends publish position only every 500 ms. Keep the wall baseline
    // until media position advances, or real listening time would be lost.
    if (position != m_lastPosition) {
        m_lastPosition = position;
        m_lastWall = now;
    }
    return ok;
}
void Playback::finish() {
    sample(true);
    m_session = {};
}
void Playback::load(qint64 id, bool autoplay, bool resume) {
    auto t = m_library->track(id);
    if (!t.id)
        return;
    if (autoplay && !m_decodeOnly && !qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS") && QMediaDevices::defaultAudioOutput().isNull()) {
        autoplay = false;
        emit error("未检测到音频输出设备，请连接耳机或音箱后再播放。");
    }
    if (!m_decodeOnly && !qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS") && !QMediaDevices::defaultAudioOutput().isNull()) {
        m_output.setDevice(QMediaDevices::defaultAudioOutput());
        m_player.setAudioOutput(&m_output);
    }
    m_loading = true;
    m_autoStart = autoplay;
    if (!resume) {
        m_threshold = qBound(70, m_library->setting("threshold", 80).toInt(), 100);
        m_session = m_library->startSession(id, t.duration);
        m_restorePosition = -1;
    }
    m_lastPosition = 0;
    m_lastWall = m_elapsed.elapsed();
    m_library->setSetting("activeSession", double(m_session.id));
    m_library->setSetting("activeThreshold", m_threshold);
    if (!QFileInfo::exists(m_library->absolute(t.file))) {
        m_loading = false;
        onError("音乐文件缺失：" + t.title);
        return;
    }
    if (!ensurePlayable(t, autoplay))
        return;
    emit trackChanged();
    saveQueue();
}
void Playback::play(const QVector<qint64> &ids, qint64 id) {
    if (m_shutdown) return;
    if (!m_decodeOnly && !qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS") && QMediaDevices::defaultAudioOutput().isNull()) {
        emit error("未检测到音频输出设备，请连接耳机或音箱后再播放。");
        return;
    }
    if (!m_decodeOnly && !qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))
        m_player.setAudioOutput(&m_output);
    sample();
    finish();
    haltTransport();
    m_failed.clear();
    m_queue.set(ids, id);
    if (m_queue.current())
        load(m_queue.current(), true);
}
void Playback::toggle() {
    if (m_shutdown) return;
    if (!m_queue.current()) {
        auto tracks = m_library->tracks();
        QVector<qint64> ids;
        for (const auto &t : tracks)
            ids << t.id;
        play(ids);
        return;
    }
    if (playing()) {
        sample();
        if (m_midi) {
#ifdef Q_OS_WIN
            mciSendStringW(L"pause relaxer_midi", nullptr, 0, nullptr);
#endif
            m_midiPlaying = false;
            emit stateChanged();
        } else
            m_player.pause();
        save();
    } else {
        if (!m_decodeOnly && !qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS") && QMediaDevices::defaultAudioOutput().isNull()) {
            emit error("未检测到音频输出设备，请连接耳机或音箱后再播放。");
            return;
        }
        if (!m_decodeOnly && !qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))
            m_player.setAudioOutput(&m_output);
        if (m_midi) {
#ifdef Q_OS_WIN
            mciSendStringW(L"play relaxer_midi", nullptr, 0, nullptr);
#endif
            m_midiPlaying = true;
            m_lastPosition = position();
            m_lastWall = m_elapsed.elapsed();
            emit stateChanged();
        } else if (m_player.mediaStatus() == QMediaPlayer::EndOfMedia || !m_session.id) {
            m_restorePosition = -1;
            load(m_queue.current(), true);
        } else
            m_player.play();
    }
}
void Playback::next() {
    if (m_shutdown) return;
    sample();
    finish();
    haltTransport();
    m_failed.clear();
    auto id = m_queue.next(true);
    if (id)
        load(id, true);
    else
        saveQueue();
}
void Playback::playUpcoming(int index){
    if (m_shutdown) return;
    const auto future=m_queue.upcoming();if(index<0||index>=future.size())return;
    sample();finish();haltTransport();m_failed.clear();qint64 id=0;
    for(int i=0;i<=index;++i)id=m_queue.next(true);
    if(id)load(id,true);else saveQueue();
}
void Playback::previous() {
    if (m_shutdown) return;
    sample();
    finish();
    haltTransport();
    m_failed.clear();
    auto id = m_queue.previous();
    if (id)
        load(id, true);
}
void Playback::seek(qint64 pos) {
    if (m_shutdown) return;
    if (m_midi) {
        if (!m_session.id)
            return;
        const bool resume = m_midiPlaying;
        sample();
        m_session.sought = true;
        m_library->saveSession(m_session, 0, m_threshold);
        pos = qBound<qint64>(0, pos, qMax<qint64>(0, m_midiDuration - 1));
#ifdef Q_OS_WIN
        const auto command = QString("seek relaxer_midi to %1").arg(pos).toStdWString();
        mciSendStringW(command.c_str(), nullptr, 0, nullptr);
        if (resume)
            mciSendStringW(L"play relaxer_midi", nullptr, 0, nullptr);
#else
        Q_UNUSED(resume);
#endif
        m_lastPosition = pos;
        m_lastWall = m_elapsed.elapsed();
        emit positionChanged(pos, m_midiDuration);
        emit stateChanged();
        saveQueue();
        return;
    }
    if (!m_session.id || !m_player.isSeekable())
        return;
    bool resume = playing();
    sample();
    m_session.sought = true;
    m_library->saveSession(m_session, 0, m_threshold);
    if (resume)
        m_player.pause();
    m_player.setPosition(qBound<qint64>(0, pos, m_player.duration()));
    m_lastPosition = m_player.position();
    m_lastWall = m_elapsed.elapsed();
    if (resume)
        m_player.play();
    emit stateChanged();
    saveQueue();
}
void Playback::setMode(Mode m) {
    if (m_shutdown) return;
    m_queue.setMode(m);
    m_library->setSetting("mode", int(m));
    emit stateChanged();
    saveQueue();
}
void Playback::setVolume(int v) {
    if (m_shutdown) return;
    m_output.setVolume(float(qBound(0, v, 100)) / 100);
    m_library->setSetting("volume", qBound(0, v, 100));
}
bool Playback::saveQueue() {
    if (m_shutdown) return m_shutdownSaved;
    QJsonArray ids;
    for (auto t : m_queue.items) ids.append(double(t));
    // Same keys and representation; one existing storage transaction prevents
    // a failed periodic/final snapshot from leaving a partially updated queue.
    const bool ok = m_library->setSettings({{"queue", ids}, {"current", double(m_queue.current())},
        {"position", double(position())}, {"activeSession", double(m_session.id)}});
    if (!ok) emit saveFailed(m_library->lastError());
    return ok;
}
bool Playback::save() {
    if (m_shutdown) return m_shutdownSaved;
    const bool sampled = sample();
    if (!sampled) emit saveFailed(m_library->lastError());
    const bool saved = saveQueue();
    return sampled && saved;
}
void Playback::restore() {
    if (m_shutdown) return;
    QVector<qint64> ids;
    for (const auto &v : m_library->setting("queue").toArray())
        if (m_library->track(qint64(v.toDouble())).id)
            ids << qint64(v.toDouble());
    auto id = qint64(m_library->setting("current").toDouble());
    m_queue.set(ids, id);
    if (!m_queue.current())
        return;
    m_session = m_library->session(qint64(m_library->setting("activeSession").toDouble()));
    m_threshold = qBound(70, m_library->setting("activeThreshold", 80).toInt(), 100);
    m_restorePosition = qint64(m_library->setting("position", 0).toDouble());
    if (m_session.track != m_queue.current() || !m_session.id) {
        m_session = m_library->startSession(m_queue.current(), m_library->track(m_queue.current()).duration);
        if (m_restorePosition > 0)
            m_session.sought = true;
    }
    load(m_queue.current(), false, true);
}
void Playback::forget(const QVector<qint64> &ids) {
    if (m_shutdown) return;
    if (ids.contains(m_queue.current())) {
        finish();
        haltTransport();
        m_player.setSource({});
    }
    QVector<qint64> remaining = m_queue.items;
    for (auto id : ids)
        remaining.removeAll(id);
    auto current = m_queue.current();
    m_queue.set(remaining, remaining.contains(current) ? current : 0);
    if (m_queue.current() && ids.contains(current))
        load(m_queue.current(), false);
    saveQueue();
    emit trackChanged();
}
QString Playback::roundStatus() const {
    if (!m_session.id)
        return "每一段音乐，都值得慢慢听。";
    if (m_session.sought)
        return "本轮已跳转进度，不计入播放次数";
    if (m_session.counted)
        return "本轮已计入播放次数";
    return QString("正常播放达到 %1% 后计入一次").arg(m_threshold);
}
void Playback::onError(const QString &text) {
    if (m_shutdown) return;
    if (!m_queue.current())
        return;
    auto id = m_queue.current();
    if (m_failed.contains(id))
        return;
    m_failed.insert(id);
    emit error(text);
    finish();
    haltTransport();
    if (m_failed.size() >= m_queue.items.size()) {
        m_loading = false;
        emit stateChanged();
        return;
    }
    QTimer::singleShot(150, this, [this, id] {
        if (m_shutdown || m_queue.current() != id)
            return;
        for (int i = 0; i < m_queue.items.size(); ++i) {
            auto next = m_queue.next(true);
            if (!next)
                break;
            if (!m_failed.contains(next)) {
                load(next, true);
                return;
            }
        }
        emit stateChanged();
    });
}
qint64 Playback::position() const {
    if (!m_midi)
        return m_player.position();
#ifdef Q_OS_WIN
    wchar_t buffer[64] = {};
    if (mciSendStringW(L"status relaxer_midi position", buffer, 64, nullptr) == 0)
        return QString::fromWCharArray(buffer).toLongLong();
#endif
    return m_lastPosition;
}
qint64 Playback::duration() const { return m_midi ? m_midiDuration : m_player.duration(); }
bool Playback::playing() const {
    if (m_shutdown) return false;
    if (m_midi)
        return m_midiPlaying;
    return m_player.playbackState() == QMediaPlayer::PlayingState;
}
QMediaPlayer::PlaybackState Playback::transportState() const {
    if (m_shutdown) return QMediaPlayer::StoppedState;
    return m_midi ? (m_midiPlaying ? QMediaPlayer::PlayingState : QMediaPlayer::PausedState) : m_player.playbackState();
}
bool Playback::seekable() const {
    if (m_midi)
        return m_midiDuration > 0;
    return m_player.isSeekable();
}
void Playback::haltTransport() {
    stopMidi();
    if (m_transcode) {
        auto *process = m_transcode.data();
        m_transcode.clear();
        process->kill();
        process->deleteLater();
    }
    m_player.stop();
}
void Playback::stopMidi() {
#ifdef Q_OS_WIN
    if (m_midi) {
        mciSendStringW(L"stop relaxer_midi", nullptr, 0, nullptr);
        mciSendStringW(L"close relaxer_midi", nullptr, 0, nullptr);
    }
#endif
    m_midi = false;
    m_midiPlaying = false;
}
bool Playback::startMidi(const QString &path, bool autoplay) {
    stopMidi();
    m_player.stop();
    m_player.setSource({});
#ifndef Q_OS_WIN
    Q_UNUSED(path);
    Q_UNUSED(autoplay);
    m_loading = false;
    onError("MIDI 需要在 Windows 上使用系统合成器播放。");
    return false;
#else
    const auto open =
        QString("open \"%1\" type sequencer alias relaxer_midi").arg(QDir::toNativeSeparators(path)).toStdWString();
    if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) != 0) {
        m_loading = false;
        onError("无法打开 MIDI：" + QFileInfo(path).fileName());
        return false;
    }
    mciSendStringW(L"set relaxer_midi time format milliseconds", nullptr, 0, nullptr);
    wchar_t buffer[64] = {};
    mciSendStringW(L"status relaxer_midi length", buffer, 64, nullptr);
    m_midiDuration = QString::fromWCharArray(buffer).toLongLong();
    m_midi = true;
    m_loading = false;
    qint64 start = 0;
    if (m_restorePosition >= 0) {
        start = qBound<qint64>(0, m_restorePosition, qMax<qint64>(0, m_midiDuration - 1));
        m_restorePosition = -1;
        const auto seek = QString("seek relaxer_midi to %1").arg(start).toStdWString();
        mciSendStringW(seek.c_str(), nullptr, 0, nullptr);
    }
    m_lastPosition = start;
    m_lastWall = m_elapsed.elapsed();
    if (autoplay) {
        mciSendStringW(L"play relaxer_midi", nullptr, 0, nullptr);
        m_midiPlaying = true;
    }
    emit positionChanged(start, m_midiDuration);
    emit stateChanged();
    return true;
#endif
}
bool Playback::ensurePlayable(const Track &track, bool autoplay) {
    const int generation = ++m_playGeneration;
    stopMidi();
    if (m_transcode) {
        auto *process = m_transcode.data();
        m_transcode.clear();
        process->kill();
        process->deleteLater();
    }
    const QString path = m_library->absolute(track.file);
    const QString suffix = QFileInfo(path).suffix();
    if (isMidiSuffix(suffix))
        return startMidi(path, autoplay);
    if (!needsTranscodedPlayback(suffix)) {
        m_player.setSource(QUrl::fromLocalFile(path));
        if (autoplay)
            m_player.play();
        return true;
    }
    QDir cache(m_library->root() + "/cache");
    cache.mkpath(".");
    const auto stamp = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    const auto key = QCryptographicHash::hash((path + QString::number(stamp)).toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString flac = cache.filePath(QString::fromLatin1(key) + ".flac");
    if (QFileInfo::exists(flac) && QFileInfo(flac).size() > 0) {
        m_player.setSource(QUrl::fromLocalFile(flac));
        if (autoplay)
            m_player.play();
        return true;
    }
    const auto exe = toolPath("ffmpeg");
    if (exe.isEmpty()) {
        m_loading = false;
        onError("这个格式需要精简 FFmpeg。请完成首次准备后再播放 " + suffix.toUpper() + "。");
        return false;
    }
    auto *process = new QProcess(this);
    m_transcode = process;
    process->setProgram(exe);
    process->setArguments({"-y", "-v", "error", "-i", path, "-map", "0:a:0", "-c:a", "flac", flac});
    connect(process, &QProcess::finished, this, [this, process, flac, autoplay, generation, id = track.id](int code, QProcess::ExitStatus status) {
        if (m_transcode == process)
            m_transcode.clear();
        process->deleteLater();
        if (generation != m_playGeneration || m_queue.current() != id)
            return;
        if (code != 0 || status != QProcess::NormalExit || !QFileInfo::exists(flac) || QFileInfo(flac).size() <= 0) {
            QFile::remove(flac);
            m_loading = false;
            onError("转码失败，暂时无法播放这种格式。");
            return;
        }
        m_player.setSource(QUrl::fromLocalFile(flac));
        if (autoplay)
            m_player.play();
    });
    process->start();
    return true;
}
} // namespace relaxer
