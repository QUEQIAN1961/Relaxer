#include "playback-facade.h"
#include "online-track-matcher.h"
#include <QMediaDevices>
#include <QSignalBlocker>

namespace relaxer {
void PlaybackFacade::playOnline(const QVector<Track> &tracks, qint64 start) {
    if(isShutdown()||tracks.isEmpty())return;
    // Keep the persisted local queue and listening session in the local engine.
    // Online IDs and expiring stream URLs live only in this session.
    if(!m_onlineActive){if(m_playback->playing())m_playback->toggle();m_playback->save();}
    if(!m_onlinePlayer){
        m_onlinePlayer=std::make_unique<QMediaPlayer>();
        if(!qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS")){
            m_onlineAudio=std::make_unique<QAudioOutput>();m_onlineAudio->setVolume(volume()/100.);
            m_onlinePlayer->setAudioOutput(m_onlineAudio.get());
        }
        connect(m_onlinePlayer.get(),&QMediaPlayer::positionChanged,this,[this](qint64 p){if(m_onlineActive){sampleOnline();notify(Notice::Position,p,duration());}});
        connect(m_onlinePlayer.get(),&QMediaPlayer::durationChanged,this,[this](qint64){if(m_onlineActive)notify(Notice::Position,position(),duration());});
        connect(m_onlinePlayer.get(),&QMediaPlayer::seekableChanged,this,[this](bool){if(m_onlineActive)notify(Notice::Seekable);});
        connect(m_onlinePlayer.get(),&QMediaPlayer::playbackStateChanged,this,[this]{if(m_onlineActive){sampleOnline(false,true);notify(Notice::State);}});
        connect(m_onlinePlayer.get(),&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus status){
            if(!m_onlineActive)return;
            if(status==QMediaPlayer::EndOfMedia){sampleOnline(true,true);if(m_onlineQueue.next(false))requestOnlineCurrent();else publish();}
            else publish();
        });
        connect(m_onlinePlayer.get(),&QMediaPlayer::errorOccurred,this,[this]{
            if(m_onlineActive)failOnlineSource(current().id,QStringLiteral("在线音频播放失败，请重新选择歌曲或刷新登录状态"));
        });
    }
    finishOnlineSession();m_onlinePlayer->stop();
    m_onlineTracks.clear();QVector<qint64> ids;
    for(const auto &track:tracks){if(track.id>0){m_onlineTracks.insert(track.id,track);ids.append(track.id);}}
    m_onlineQueue.mode=m_playback->mode();m_onlineQueue.set(ids,start);m_onlineActive=!ids.isEmpty();
    if(m_onlineActive)requestOnlineCurrent();
}
void PlaybackFacade::requestOnlineCurrent(){
    if(!m_onlineActive||isShutdown())return;
    finishOnlineSession();
    {const QSignalBlocker blocker(m_onlinePlayer.get());m_onlinePlayer->stop();m_onlinePlayer->setSource({});}
    m_onlinePending=true;beginAttempt(current().id);
    beginOnlineSession();
    emit trackChanged();emit stateChanged();publish();emit onlineSourceRequested(current().id);
}
void PlaybackFacade::setOnlineSource(qint64 id,const QUrl &url){
    if(!m_onlineActive||isShutdown()||current().id!=id||url.isEmpty())return;
    m_onlinePending=false;m_onlinePlayer->setSource(url);m_onlinePlayer->play();publish();
}
void PlaybackFacade::updateOnlineTrack(const Track &track){
    if(!m_onlineTracks.contains(track.id))return;
    m_onlineTracks.insert(track.id,track);
    emit onlineTrackUpdated(track.id);
    if(m_onlineActive)publish();
}
void PlaybackFacade::failOnlineSource(qint64 id,const QString &message){
    if(!m_onlineActive||isShutdown()||current().id!=id)return;
    m_onlinePending=false;emit playbackFailure(m_operation,id,current().title,message,-1);emit error(message);publish();
}
void PlaybackFacade::stopOnline(){
    if(!m_onlineActive)return;
    finishOnlineSession();
    m_onlineActive=false;m_onlinePending=false;
    if(m_onlinePlayer){const QSignalBlocker blocker(m_onlinePlayer.get());m_onlinePlayer->stop();m_onlinePlayer->setSource({});}
    emit trackChanged();emit stateChanged();publish();
}
void PlaybackFacade::beginOnlineSession(){
    auto track=current();
    if(!m_matchCandidatesReady){m_matchCandidates=m_library->tracks();m_matchCandidatesReady=true;}
    if(!m_onlineMatches.contains(track.hash))m_onlineMatches.insert(track.hash,matchOnlineTrack(track,m_matchCandidates));
    track.matchedLocalId=m_onlineMatches.value(track.hash);m_onlineTracks.insert(track.id,track);
    m_onlineSessionMatched=track.matchedLocalId>0;m_onlineThreshold=qBound(70,m_library->setting("threshold",80).toInt(),100);
    m_onlineSession=m_onlineSessionMatched?m_library->startSession(track.matchedLocalId,track.duration):m_library->startOnlineSession(track);
    m_onlineLastWall=m_onlineClock.elapsed();m_onlineLastPosition=0;
}
void PlaybackFacade::sampleOnline(bool finishing,bool flush){
    if(!m_onlineSession.id||!m_onlinePlayer)return;
    const auto now=m_onlineClock.elapsed(),pos=m_onlinePlayer->position();qint64 delta=pos-m_onlineLastPosition;
    if(!m_onlinePending&&(playing()||finishing||flush)&&delta>0)delta=qMin(delta,qMax<qint64>(0,now-m_onlineLastWall)+80);else delta=0;
    if(m_onlineSession.duration<=0&&m_onlinePlayer->duration()>0)m_onlineSession.duration=m_onlinePlayer->duration();
    if(delta>0||finishing){const bool ok=m_onlineSessionMatched?m_library->saveSession(m_onlineSession,delta,m_onlineThreshold,finishing):m_library->saveOnlineSession(m_onlineSession,delta,m_onlineThreshold,finishing);if(!ok)emit saveFailed("在线收听统计未能保存");}
    if(pos!=m_onlineLastPosition){m_onlineLastPosition=pos;m_onlineLastWall=now;}
}
void PlaybackFacade::finishOnlineSession(){if(!m_onlineSession.id)return;sampleOnline(m_onlinePlayer&&m_onlinePlayer->mediaStatus()==QMediaPlayer::EndOfMedia,true);m_onlineSession={};}
}
