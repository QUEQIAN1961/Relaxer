#include "playback-bridge.h"
#include <QThread>
#include <QFileInfo>

namespace relaxer {
PlaybackBridge::PlaybackBridge(PlaybackFacade *facade, QObject *parent, Library *library)
    : QObject(parent), m_facade(facade), m_library(library) {
    Q_ASSERT(facade && facade->thread() == thread());
    connect(facade, &PlaybackFacade::snapshotChanged, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::saved, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::error, this, &PlaybackBridge::setError);
    connect(facade, &PlaybackFacade::saveFailed, this, &PlaybackBridge::setError);
    connect(facade, &PlaybackFacade::trackChanged, this, [this] { setError({}); });
    const auto invalidate = [this] { m_facade = nullptr; refresh(); };
    // The legacy facade disconnects every outgoing connection at the start of
    // its destructor. Its owned backend still emits destroyed, before facade
    // QObject destruction, so use that lifetime notification as well.
    connect(facade->backend(), &QObject::destroyed, this, invalidate);
    refresh();
}
void PlaybackBridge::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto next = m_facade ? m_facade->snapshot() : PlaybackSnapshot{};
    const auto previous = m_state;
    const bool wasAvailable = m_available;
    const bool wasPlayable = m_canPlay;
    m_state = next;
    m_available = available();
    m_canPlay = available() && next.track.id && next.mediaStatus != QMediaPlayer::InvalidMedia &&
        (!m_library || QFileInfo::exists(m_library->absolute(next.track.file)));
    if (next.volume > 0) m_lastVolume = next.volume;
    if (previous.track.id != next.track.id || previous.track.title != next.track.title ||
        previous.track.artist != next.track.artist) emit trackChanged();
    if (previous.playing != next.playing) emit playingChanged();
    if (previous.position != next.position || previous.duration != next.duration) emit progressChanged();
    if (previous.volume != next.volume) emit volumeChanged();
    if (previous.mode != next.mode) emit modeChanged();
    if (previous.seekable != next.seekable || previous.duration != next.duration || wasPlayable != m_canPlay ||
        previous.track.id != next.track.id || wasAvailable != m_available) emit capabilitiesChanged();
    if (wasAvailable != m_available) emit availableChanged();
}
void PlaybackBridge::toggle() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (available() && m_facade->current().id) m_facade->toggle();
}
void PlaybackBridge::play() { if (available() && hasTrack() && !m_facade->playing()) { m_facade->toggle(); refresh(); } }
void PlaybackBridge::pause() { if (available() && m_facade->playing()) { m_facade->toggle(); refresh(); } }
void PlaybackBridge::next() { if (available()) { m_facade->next(); refresh(); } }
void PlaybackBridge::previous() { if (available()) { m_facade->previous(); refresh(); } }
void PlaybackBridge::seek(qint64 position) {
    if (available() && m_facade->seekable() && m_facade->duration() > 0) {
        m_facade->seek(qBound<qint64>(0, position, m_facade->duration())); refresh();
    }
}
void PlaybackBridge::setVolume(int volume) { if (available()) { m_facade->setVolume(qBound(0,volume,100)); refresh(); } }
void PlaybackBridge::toggleMute() {
    if (!available()) return;
    if (m_facade->volume() > 0) {
        m_lastVolume = m_facade->volume();
        if (m_library) m_library->setSetting("qml.lastAudibleVolume",m_lastVolume);
        setVolume(0);
    } else {
        const int last = m_library ? m_library->setting("qml.lastAudibleVolume",m_lastVolume).toInt(m_lastVolume) : m_lastVolume;
        setVolume(qBound(1,last,100));
    }
}
void PlaybackBridge::setMode(int mode) {
    if (available() && mode >= int(Mode::Sequential) && mode <= int(Mode::Shuffle)) {
        m_facade->setMode(Mode(mode)); refresh();
    }
}
void PlaybackBridge::playQueued(const QString &id) {
    if (!available()) return;
    const auto items = m_facade->queue().items;
    if (items.contains(id.toLongLong())) { m_facade->play(items,id.toLongLong()); refresh(); }
}
void PlaybackBridge::removeFromQueue(const QString &id) {
    if (available() && m_facade->queue().items.contains(id.toLongLong())) { m_facade->forget({id.toLongLong()}); refresh(); }
}
void PlaybackBridge::setError(const QString &error) {
    if (m_error != error) { m_error = error; emit errorChanged(); }
}
}
