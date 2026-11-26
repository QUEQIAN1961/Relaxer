#include "playback-bridge.h"
#include <QThread>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>

namespace relaxer {
PlaybackBridge::PlaybackBridge(PlaybackFacade *facade, QObject *parent, Library *library)
    : QObject(parent), m_facade(facade), m_library(library) {
    Q_ASSERT(facade && facade->thread() == thread());
    m_muted = facade->volume() == 0;
    connect(facade,&PlaybackFacade::audioOutputsChanged,this,&PlaybackBridge::audioOutputsChanged);
    if (m_muted && library)
        m_muteRestoreVolume = qBound(0, library->setting("qml.muteRestoreVolume", 0).toInt(), 100);
    connect(facade, &PlaybackFacade::snapshotChanged, this, [this](const PlaybackSnapshot &value) { applySnapshot(value); });
    connect(facade, &PlaybackFacade::saved, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::playbackFailure, this,
        [this](quint64 operation, qint64 target, const QString &title, const QString &message, int code) {
            setError(message); refresh();
            emit failure(operation, QString::number(target), title, message, code);
        });
    connect(facade, &PlaybackFacade::saveFailed, this, [this](const QString &message) {
        const auto reason = message.isEmpty() ? QStringLiteral("播放状态保存失败") : message;
        if (m_saveError == reason) return; // periodic save: one outstanding failure until recovery
        m_saveError = reason; emit persistenceFailure(reason);
    });
    connect(facade, &PlaybackFacade::saved, this, [this](bool ok) { if(ok) m_saveError.clear(); });
    connect(facade, &PlaybackFacade::playbackAttemptStarted, this, [this] { setError({}); emit recovered(); });
    connect(facade, &PlaybackFacade::playbackRecovered, this, [this] { setError({}); });
    if (library) {
        Q_ASSERT(library->thread() == thread());
        connect(library, &Library::changed, this, &PlaybackBridge::refreshMetadata);
        connect(library,&Library::onlineFavoritesChanged,this,&PlaybackBridge::refreshMetadata);
        connect(library, &QObject::destroyed, this, [this] { m_library = nullptr; refreshMetadata(); });
    }
    const auto invalidate = [this] { m_facade = nullptr; refresh(); };
    // The legacy facade disconnects every outgoing connection at the start of
    // its destructor. Its owned backend still emits destroyed, before facade
    // QObject destruction, so use that lifetime notification as well.
    connect(facade->backend(), &QObject::destroyed, this, invalidate);
    refresh();
}
void PlaybackBridge::playHistory(const QVariantMap &row){
    if(!m_facade)return;
    if(!row.value("cloud").toBool()){m_facade->play({row.value("trackId").toString().toLongLong()},0,{{"provider","local"},{"localKey","0"}});return;}
    Track track;track.id=row.value("trackId").toString().toLongLong();track.hash=row.value("hash").toString();track.title=row.value("title").toString();track.artist=row.value("artist").toString();track.album=row.value("album").toString();track.cover=row.value("cover").toString();track.duration=row.value("duration").toLongLong();track.onlineMetadata=QJsonObject::fromVariantMap(row.value("metadata").toMap());
    if(track.id>0)m_facade->playOnline({track},track.id);
}
void PlaybackBridge::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    applySnapshot(m_facade ? m_facade->snapshot() : PlaybackSnapshot{});
}
void PlaybackBridge::applySnapshot(const PlaybackSnapshot &next) {
    Q_ASSERT(QThread::currentThread() == thread());
    // A queued payload can outlive a newer synchronous UI action. Keep the
    // cheap value delivery, but accept only the authoritative engine revision.
    if (m_facade && next.revision != m_facade->snapshotRevision()) return;
    const auto previous = m_state;
    const auto previousTitle = title(), previousArtist = artist();
    const bool previousFavorite = currentFavorite();
    const auto previousCover = m_coverUrl;
    const auto previousRevision = m_artworkRevision;
    const bool wasAvailable = m_available;
    const bool wasPlayable = m_canPlay;
    m_state = next;
    if (previous.track.id != next.track.id || previous.track.hash != next.track.hash || previous.track.title != next.track.title ||
        previous.track.artist != next.track.artist || previous.track.cover != next.track.cover || previous.track.favorite != next.track.favorite)
        readMetadata();
    m_available = available();
    // A failed/missing resource remains retryable after the user repairs it.
    m_canPlay = available() && next.track.id;
    if (previous.volume != next.volume) {
        m_muted = next.volume == 0;
        if (next.volume > 0 && m_muteRestoreVolume) {
            m_muteRestoreVolume = 0;
            if (m_library) m_library->setSetting("qml.muteRestoreVolume", 0);
        }
    }
    if (previous.track.id != next.track.id || previousTitle != title() || previousArtist != artist()) emit trackChanged();
    if (previousFavorite != currentFavorite()) emit currentFavoriteChanged();
    if (previousCover != m_coverUrl || previousRevision != m_artworkRevision) emit artworkChanged();
    if (previous.playing != next.playing) emit playingChanged();
    if (previous.position != next.position || previous.duration != next.duration) emit progressChanged();
    if (previous.buffered != next.buffered) emit bufferChanged();
    if (previous.volume != next.volume) emit volumeChanged();
    if (previous.mode != next.mode) emit modeChanged();
    if (previous.track.onlineMetadata.value("preview")!=next.track.onlineMetadata.value("preview") || previous.video != next.video || previous.videoFrameReady!=next.videoFrameReady || previous.onlineVideoVisible!=next.onlineVideoVisible || previous.seekable != next.seekable || previous.duration != next.duration || wasPlayable != m_canPlay ||
        previous.track.id != next.track.id || wasAvailable != m_available) emit capabilitiesChanged();
    if (wasAvailable != m_available) emit availableChanged();
}
QVariantList PlaybackBridge::bufferedRanges() const {
    QVariantList ranges;
    for (const auto &range : m_state.buffered.intervals())
        ranges.append(QVariantMap{{"start", range.start()}, {"end", range.end()}});
    return ranges;
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
bool PlaybackBridge::nonInteractiveExit() const {
    return QCoreApplication::instance()->property("relaxerNonInteractiveExit").toBool();
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
    if(!available()||!m_library)return;
    const auto track=m_facade->current();
    if(!track.id)return;
    const bool saved=isOnlineTrack(track)?m_library->setTrackFavorites({track},!m_library->onlineFavorite(track.hash)):
        m_library->favorite(track.id,!track.favorite);
    if(!saved)emit persistenceFailure(m_library->lastError());
    else refresh();
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
    const bool online=isOnlineTrack(m_state.track);
    m_metadata=m_library&&m_state.track.id&&!online?m_library->track(m_state.track.id):m_state.track;
    if(m_library&&online)m_metadata.favorite=m_library->onlineFavorite(m_metadata.hash);
    if (!m_library) { m_metadata.cover.clear(); m_metadata.favorite = false; }
    const QUrl source(m_metadata.cover);
    m_coverUrl = source.scheme()=="https"||source.scheme()=="http" ? source : m_library && !m_metadata.cover.isEmpty() ? QUrl::fromLocalFile(m_library->absolute(m_metadata.cover)) : QUrl();
    if(!m_coverUrl.isEmpty()&&!m_coverUrl.isLocalFile()){m_artworkRevision=m_coverUrl.toString();return;}
    const QFileInfo image(m_coverUrl.toLocalFile());
    m_artworkRevision = m_coverUrl.isEmpty() ? QString() : QString::number(image.lastModified().toMSecsSinceEpoch()) + ':' + QString::number(image.size());
}
void PlaybackBridge::seek(qint64 position) {
    if (available() && m_facade->seekable() && m_facade->duration() > 0) {
        m_facade->seek(qBound<qint64>(0, position, m_facade->duration())); refresh();
    }
}
void PlaybackBridge::setVolume(int volume) {
    if (!available()) return;
    const int previousVolume = this->volume();
    const bool wasMuted = m_muted;
    volume = qBound(0, volume, 100);
    // Moving the slider explicitly replaces any volume saved by the mute button.
    if (m_muteRestoreVolume) {
        m_muteRestoreVolume = 0;
        if (m_library) m_library->setSetting("qml.muteRestoreVolume", 0);
    }
    m_muted = volume == 0;
    m_facade->setVolume(volume);
    refresh();
    if (previousVolume == this->volume() && wasMuted != m_muted) emit volumeChanged();
}
void PlaybackBridge::toggleMute() {
    if (!available()) return;
    const int previousVolume = volume();
    const int nextVolume = m_muted ? m_muteRestoreVolume : 0;
    m_muteRestoreVolume = m_muted ? 0 : previousVolume;
    m_muted = !m_muted;
    if (m_library) m_library->setSetting("qml.muteRestoreVolume", m_muteRestoreVolume);
    m_facade->setVolume(nextVolume);
    refresh();
    // Unmuting a manually zeroed slider changes the icon without raising volume.
    if (previousVolume == volume()) emit volumeChanged();
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
