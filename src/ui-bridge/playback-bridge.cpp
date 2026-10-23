#include "playback-bridge.h"
#include <QThread>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>

namespace relaxer {
PlaybackBridge::PlaybackBridge(PlaybackFacade *facade, QObject *parent, Library *library)
    : QObject(parent), m_facade(facade), m_library(library) {
    Q_ASSERT(facade && facade->thread() == thread());
    connect(facade, &PlaybackFacade::snapshotChanged, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::saved, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::error, this, &PlaybackBridge::setError);
    connect(facade, &PlaybackFacade::saveFailed, this, &PlaybackBridge::setError);
    connect(facade, &PlaybackFacade::trackChanged, this, [this] { setError({}); });
    if (library) {
        Q_ASSERT(library->thread() == thread());
        connect(library, &Library::changed, this, &PlaybackBridge::refreshMetadata);
        connect(library, &QObject::destroyed, this, [this] { m_library = nullptr; refreshMetadata(); });
    }
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
    const auto previousTitle = title(), previousArtist = artist();
    const bool previousFavorite = currentFavorite();
    const auto previousCover = m_coverUrl;
    const auto previousRevision = m_artworkRevision;
    const bool wasAvailable = m_available;
    const bool wasPlayable = m_canPlay;
    m_state = next;
    if (previous.track.id != next.track.id || previous.track.title != next.track.title ||
        previous.track.artist != next.track.artist || previous.track.cover != next.track.cover)
        readMetadata();
    m_available = available();
    m_canPlay = available() && next.track.id && next.mediaStatus != QMediaPlayer::InvalidMedia &&
        (!m_library || QFileInfo::exists(m_library->absolute(next.track.file)));
    if (next.volume > 0) m_lastVolume = next.volume;
    if (previous.track.id != next.track.id || previousTitle != title() || previousArtist != artist()) emit trackChanged();
    if (previousFavorite != currentFavorite()) emit currentFavoriteChanged();
    if (previousCover != m_coverUrl || previousRevision != m_artworkRevision) emit artworkChanged();
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
bool PlaybackBridge::requestExit(int decision){
    if(!m_facade)return true;
    if(QCoreApplication::instance()->property("relaxerNonInteractiveExit").toBool())decision=1;
    QPointer<PlaybackBridge> guard(this);
    const bool closed=m_facade->requestExit(static_cast<PlaybackFacade::ExitDecision>(decision));
    if(guard)guard->refresh();return closed;
}
void PlaybackBridge::pause() { if (available() && m_facade->playing()) { m_facade->toggle(); refresh(); } }
void PlaybackBridge::next() { if (available()) { beginTransition(1); m_facade->next(); refresh(); } }
void PlaybackBridge::previous() { if (available()) { beginTransition(-1); m_facade->previous(); refresh(); } }
void PlaybackBridge::playUpcoming(int index) {
    if (!available()) return;
    const auto forecast = m_facade->upcoming();
    if (index < 0 || index >= forecast.size()) return;
    beginTransition(0); m_facade->playUpcoming(index); refresh();
}
void PlaybackBridge::toggleFavorite() {
    if (!available() || !m_library || !hasTrack()) return;
    const auto track = m_library->track(m_state.track.id);
    if (track.id && !m_library->favorite(track.id, !track.favorite)) setError(m_library->lastError());
}
void PlaybackBridge::beginTransition(int direction) {
    if (m_transitionDirection != direction) { m_transitionDirection = direction; emit transitionDirectionChanged(); }
    emit transitionStarted(direction);
}
void PlaybackBridge::refreshMetadata() {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto previousTitle = title(), previousArtist = artist();
    const bool previousFavorite = currentFavorite();
    const auto previousCover = m_coverUrl;
    const auto previousRevision = m_artworkRevision;
    readMetadata();
    if (previousFavorite != currentFavorite()) emit currentFavoriteChanged();
    if (previousCover != m_coverUrl || previousRevision != m_artworkRevision) emit artworkChanged();
    if (previousTitle != title() || previousArtist != artist()) emit trackChanged();
}
void PlaybackBridge::readMetadata() {
    m_metadata = m_library && m_state.track.id ? m_library->track(m_state.track.id) : m_state.track;
    if (!m_library) { m_metadata.cover.clear(); m_metadata.favorite = false; }
    m_coverUrl = m_library && !m_metadata.cover.isEmpty() ? QUrl::fromLocalFile(m_library->absolute(m_metadata.cover)) : QUrl();
    const QFileInfo image(m_coverUrl.toLocalFile());
    m_artworkRevision = m_coverUrl.isEmpty() ? QString() : QString::number(image.lastModified().toMSecsSinceEpoch()) + ':' + QString::number(image.size());
}
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
