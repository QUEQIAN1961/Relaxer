#include "playback-facade.h"

namespace relaxer {
PlaybackFacade::PlaybackFacade(Library *library, QObject *parent, bool decodeOnly)
    : QObject(parent), m_playback(new Playback(library, this, decodeOnly)), m_library(library), m_saveTimer(this) {
    Q_ASSERT(library && library->thread() == thread());
    m_onlineClock.start();connect(library,&Library::changed,this,[this]{m_matchCandidatesReady=false;m_onlineMatches.clear();});
    connect(m_playback, &Playback::trackChanged, this, [this] { if(!m_onlineActive)notify(Notice::Track); });
    connect(m_playback, &Playback::stateChanged, this, [this] { if(!m_onlineActive)notify(Notice::State); });
    connect(m_playback, &Playback::positionChanged, this,
            [this](qint64 pos, qint64 duration) { if(!m_onlineActive)notify(Notice::Position, pos, duration); });
    connect(m_playback, &Playback::error, this,
            [this](const QString &message) { if(!m_onlineActive)notify(Notice::Error, 0, 0, message); });
    connect(m_playback, &Playback::saveFailed, this, &PlaybackFacade::saveFailed, Qt::QueuedConnection);
    connect(m_playback->media(), &QMediaPlayer::seekableChanged, this,
            [this](bool) { if(!m_onlineActive)notify(Notice::Seekable); });
    connect(m_playback->media(), &QMediaPlayer::mediaStatusChanged, this,
            [this] { if(!m_onlineActive)notify(Notice::Snapshot); });
    m_saveTimer.setObjectName("playbackPersistenceTimer");
    m_saveTimer.setInterval(4000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] { save(); });
    m_saveTimer.start();
}
PlaybackFacade::~PlaybackFacade() {
    disconnect(this, nullptr, nullptr, nullptr);
    shutdown(true);
    m_onlinePlayer.reset();m_onlineAudio.reset();
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
    value.transport = m_onlineActive && m_onlinePlayer ? m_onlinePlayer->playbackState() : m_playback->transportState();
    value.mediaStatus = m_onlineActive && m_onlinePlayer ? m_onlinePlayer->mediaStatus() : m_playback->media()->mediaStatus();
    return value;
}
void PlaybackFacade::notify(Notice notice, qint64 pos, qint64 duration, const QString &message) {
    if (isShutdown())
        return;
    const auto value = snapshot();
    if (notice == Notice::State && value.playing && m_failedTrack) {
        m_failedTrack = 0;
        emit playbackRecovered();
    }
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
    case Notice::Error: {
        const auto target = m_requestedTrack ? track(m_requestedTrack) : value.track;
        const auto key = QString::number(target.id) + ':' + message;
        m_failedTrack = target.id;
        if (!m_reportedFailures.contains(key)) {
            if (m_reportedFailures.size() >= 64) m_reportedFailures.clear();
            m_reportedFailures.insert(key);
            emit playbackFailure(m_operation, target.id, target.title, message, int(m_playback->media()->error()));
        }
        emit error(message);
        break;
    }
    case Notice::Snapshot:
        break;
    }
}
void PlaybackFacade::publish() {
    if (!isShutdown())
        emit snapshotChanged(snapshot());
}
QVector<qint64> PlaybackFacade::upcoming() {
    return isShutdown() ? QVector<qint64>{} : m_onlineActive ? m_onlineQueue.upcoming() : m_playback->upcoming();
}
void PlaybackFacade::beginAttempt(qint64 requested) {
    ++m_operation; m_requestedTrack = requested; m_reportedFailures.clear();
    emit playbackAttemptStarted(m_operation);
}
void PlaybackFacade::play(const QVector<qint64> &ids, qint64 start) {
    if (!isShutdown()) {
        if(m_onlineActive&&!ids.isEmpty()&&m_onlineTracks.contains(ids.first())){m_onlineQueue.set(ids,start);requestOnlineCurrent();return;}
        stopOnline();
        beginAttempt(start ? start : ids.value(0));
        m_playback->play(ids, start);
        m_requestedTrack = 0;
        publish();
    }
}
void PlaybackFacade::toggle() {
    if (!isShutdown()) {
        if(m_onlineActive){
            if(m_onlinePending||m_onlinePlayer->source().isEmpty()||m_onlinePlayer->mediaStatus()==QMediaPlayer::EndOfMedia||m_onlinePlayer->mediaStatus()==QMediaPlayer::InvalidMedia)requestOnlineCurrent();
            else if(playing())m_onlinePlayer->pause();else m_onlinePlayer->play();
            publish();return;
        }
        const bool retry = !playing() && m_failedTrack == current().id && m_failedTrack;
        if (!playing()) beginAttempt(current().id);
        // An explicit retry uses the existing play command, which resets the engine's
        // failed-track guard. No change to automatic skip or transport algorithms.
        if (retry) m_playback->play(queue().items, current().id);
        else m_playback->toggle();
        m_requestedTrack = 0;
        publish();
    }
}
void PlaybackFacade::next() {
    if (!isShutdown()) {
        if(m_onlineActive){if(m_onlineQueue.next(true))requestOnlineCurrent();return;}
        beginAttempt();
        m_playback->next();
        publish();
    }
}
void PlaybackFacade::previous() {
    if (!isShutdown()) {
        if(m_onlineActive){if(m_onlineQueue.previous())requestOnlineCurrent();return;}
        beginAttempt();
        m_playback->previous();
        publish();
    }
}
void PlaybackFacade::playUpcoming(int index) {
    if (!isShutdown()) {
        if(m_onlineActive){const auto list=m_onlineQueue.upcoming();if(index<0||index>=list.size())return;for(int i=0;i<=index;++i)m_onlineQueue.next(true);requestOnlineCurrent();return;}
        beginAttempt();
        m_playback->playUpcoming(index);
        publish();
    }
}
void PlaybackFacade::seek(qint64 pos) {
    if (!isShutdown()) {
        if(m_onlineActive){if(seekable()){sampleOnline(false,true);m_onlineSession.sought=true;if(m_onlineSession.id){if(m_onlineSessionMatched)m_library->saveSession(m_onlineSession,0,m_onlineThreshold);else m_library->saveOnlineSession(m_onlineSession,0,m_onlineThreshold);}const auto target=qBound<qint64>(0,pos,duration());m_onlineLastPosition=target;m_onlineLastWall=m_onlineClock.elapsed();m_onlinePlayer->setPosition(target);}publish();return;}
        m_playback->seek(pos);
        publish();
    }
}
void PlaybackFacade::setMode(Mode mode) {
    if(m_onlineActive)m_onlineQueue.setMode(mode);
    if (!isShutdown())
        m_playback->setMode(mode);
}
void PlaybackFacade::setVolume(int volume) {
    if(m_onlineAudio)m_onlineAudio->setVolume(qBound(0,volume,100)/100.);
    if (!isShutdown()) {
        m_playback->setVolume(volume);
        publish();
    }
}
void PlaybackFacade::forget(const QVector<qint64> &ids) {
    if(m_onlineActive){
        auto remaining=m_onlineQueue.items;const auto before=m_onlineQueue.current();for(auto id:ids)remaining.removeAll(id);
        m_onlineQueue.set(remaining,remaining.contains(before)?before:0);
        if(remaining.isEmpty()){stopOnline();emit trackChanged();publish();}
        else if(before!=m_onlineQueue.current())requestOnlineCurrent();
        else{emit stateChanged();publish();}
        return;
    }
    if (!isShutdown())
        m_playback->forget(ids);
}
void PlaybackFacade::restore() {
    if (!isShutdown()) {
        beginAttempt();
        m_playback->restore();
        publish();
    }
}
bool PlaybackFacade::save() {
    if(m_onlineActive)sampleOnline(false,true);
    if (isShutdown())
        return m_playback->shutdown();
    const bool ok = m_playback->save();
    if(!ok||m_playback->lastSaveWrote())emit saved(ok);
    return ok;
}
bool PlaybackFacade::shutdown(bool force) {
    if (isShutdown() || m_shuttingDown)
        return m_playback->isShutdown()?m_playback->shutdown():false;
    m_shuttingDown = true;
    finishOnlineSession();
    const bool ok = m_playback->shutdown(force);
    if(isShutdown()){m_saveTimer.stop();if(m_onlinePlayer)m_onlinePlayer->stop();}
    else m_shuttingDown=false;
    if(!ok||m_playback->lastSaveWrote())emit saved(ok);
    return ok;
}
bool PlaybackFacade::requestExit(ExitDecision decision){
    if(decision==ExitDecision::Cancel)return false;
    if(decision!=ExitDecision::Retry&&decision!=ExitDecision::ExitAnyway)return false;
    QPointer<PlaybackFacade> guard(this);
    shutdown(decision==ExitDecision::ExitAnyway);
    return !guard||guard->isShutdown();
}
} // namespace relaxer
