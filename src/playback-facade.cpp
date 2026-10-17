#include "playback-facade.h"

namespace relaxer {
PlaybackFacade::PlaybackFacade(Library *library, QObject *parent, bool decodeOnly)
    : QObject(parent), m_playback(new Playback(library, this, decodeOnly)), m_saveTimer(this) {
    Q_ASSERT(library && library->thread() == thread());
    connect(m_playback, &Playback::trackChanged, this, [this] { notify(Notice::Track); });
    connect(m_playback, &Playback::stateChanged, this, [this] { notify(Notice::State); });
    connect(m_playback, &Playback::positionChanged, this,
            [this](qint64 pos, qint64 duration) { notify(Notice::Position, pos, duration); });
    connect(m_playback, &Playback::error, this,
            [this](const QString &message) { notify(Notice::Error, 0, 0, message); });
    connect(m_playback, &Playback::saveFailed, this, &PlaybackFacade::saveFailed, Qt::QueuedConnection);
    connect(m_playback->media(), &QMediaPlayer::seekableChanged, this,
            [this](bool) { notify(Notice::Seekable); });
    connect(m_playback->media(), &QMediaPlayer::mediaStatusChanged, this,
            [this] { notify(Notice::Snapshot); });
    m_saveTimer.setObjectName("playbackPersistenceTimer");
    m_saveTimer.setInterval(4000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] { save(); });
    m_saveTimer.start();
}
PlaybackFacade::~PlaybackFacade() {
    disconnect(this, nullptr, nullptr, nullptr);
    shutdown();
    // Release media/process children before QObject deletes other facade children.
    delete m_playback;
}
PlaybackSnapshot PlaybackFacade::snapshot() const {
    PlaybackSnapshot value;
    value.track = current();
    value.position = position();
    value.duration = duration();
    value.playing = playing();
    value.seekable = seekable();
    value.shutdown = isShutdown();
    value.volume = volume();
    value.mode = mode();
    value.queue = queue().items;
    // MIDI transport is represented by engine playing(), not the idle Qt backend.
    value.transport = m_playback->transportState();
    value.mediaStatus = m_playback->media()->mediaStatus();
    return value;
}
void PlaybackFacade::notify(Notice notice, qint64 pos, qint64 duration, const QString &message) {
    if (isShutdown())
        return;
    const auto value = snapshot();
    // New value notifications run after the engine unwinds. Keep the original
    // UI signal timing: animations and immediate error toasts depend on it.
    QMetaObject::invokeMethod(
        this,
        [this, value] {
            if (!isShutdown())
                emit snapshotChanged(value);
        },
        Qt::QueuedConnection);
    switch (notice) {
    case Notice::Track:
        emit trackChanged();
        break;
    case Notice::State:
        emit stateChanged();
        break;
    case Notice::Position:
        emit positionChanged(pos, duration);
        break;
    case Notice::Seekable:
        emit seekableChanged(value.seekable);
        break;
    case Notice::Error:
        emit error(message);
        break;
    case Notice::Snapshot:
        break;
    }
}
void PlaybackFacade::publish() {
    if (!isShutdown())
        emit snapshotChanged(snapshot());
}
QVector<qint64> PlaybackFacade::upcoming() {
    return isShutdown() ? QVector<qint64>{} : m_playback->upcoming();
}
void PlaybackFacade::play(const QVector<qint64> &ids, qint64 start) {
    if (!isShutdown()) {
        m_playback->play(ids, start);
        publish();
    }
}
void PlaybackFacade::toggle() {
    if (!isShutdown()) {
        m_playback->toggle();
        publish();
    }
}
void PlaybackFacade::next() {
    if (!isShutdown()) {
        m_playback->next();
        publish();
    }
}
void PlaybackFacade::previous() {
    if (!isShutdown()) {
        m_playback->previous();
        publish();
    }
}
void PlaybackFacade::playUpcoming(int index) {
    if (!isShutdown()) {
        m_playback->playUpcoming(index);
        publish();
    }
}
void PlaybackFacade::seek(qint64 pos) {
    if (!isShutdown()) {
        m_playback->seek(pos);
        publish();
    }
}
void PlaybackFacade::setMode(Mode mode) {
    if (!isShutdown())
        m_playback->setMode(mode);
}
void PlaybackFacade::setVolume(int volume) {
    if (!isShutdown()) {
        m_playback->setVolume(volume);
        publish();
    }
}
void PlaybackFacade::forget(const QVector<qint64> &ids) {
    if (!isShutdown())
        m_playback->forget(ids);
}
void PlaybackFacade::restore() {
    if (!isShutdown()) {
        m_playback->restore();
        publish();
    }
}
bool PlaybackFacade::save() {
    if (isShutdown())
        return m_playback->shutdown();
    const bool ok = m_playback->save();
    emit saved(ok);
    return ok;
}
bool PlaybackFacade::shutdown() {
    if (isShutdown() || m_shuttingDown)
        return m_playback->shutdown();
    m_shuttingDown = true;
    m_saveTimer.stop();
    const bool ok = m_playback->shutdown();
    emit saved(ok);
    return ok;
}
} // namespace relaxer
