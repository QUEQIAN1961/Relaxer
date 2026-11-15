#include "playback-facade.h"
#include <QVideoFrame>
#include "online-track-matcher.h"
#include "disk-cache.h"
#include <QMediaDevices>
#include <QAudioBuffer>
#include <QRandomGenerator>
#include <QJsonArray>

namespace relaxer {
bool PlaybackFacade::batchOnline(const QVector<Track> &tracks,const QString &action){
    if(isShutdown()||tracks.isEmpty())return false;
    for(const auto &track:tracks)if(track.id<=0||
        !(track.hash.startsWith("netease:")||track.hash.startsWith("qqmusic:")||track.hash.startsWith("kugou:")||track.hash.startsWith("qishui:")||track.hash.startsWith("bilibili:")))return false;
    if(action=="play"||action=="shuffle"){
        const auto first=action=="shuffle"?QRandomGenerator::global()->bounded(int(tracks.size())):0;
        playOnline(tracks,tracks[first].id);if(action=="shuffle")setMode(Mode::Shuffle);return true;
    }
    if(action!="next"&&action!="append")return false;
    return enqueueTracks(tracks,action=="next");
}
bool PlaybackFacade::enqueueTracks(const QVector<Track> &tracks,bool next){
    if(isShutdown()||tracks.isEmpty())return false;
    for(const auto &item:tracks)if(item.id<=0||(!isOnline(item)&&!m_library->track(item.id).id))return false;
    QVector<qint64> ids;for(const auto &item:tracks)if(item.id!=current().id)ids.append(item.id);
    if(ids.isEmpty())return false;
    if(!m_sharedQueueActive){m_onlineQueue=m_playback->queue();m_sharedQueueActive=true;m_playback->setExternalQueue(true);}
    const bool hadCurrent=m_onlineQueue.current()>0;
    for(const auto &item:tracks)if(item.id!=current().id&&isOnline(item))m_onlineTracks.insert(item.id,item);
    if(!m_onlineQueue.enqueue(ids,next))return false;
    m_queueFailures.clear();
    if(!hadCurrent){m_onlineActive=isOnline(current());if(m_onlineActive){ensureOnlinePlayer();m_onlinePending=false;m_onlineResumePosition=0;}else m_playback->restoreQueued(current().id,0);emit trackChanged();}
    emit stateChanged();publish();return true;
}
void PlaybackFacade::activateQueuedCurrent(){
    if(isShutdown()||!m_sharedQueueActive||!current().id)return;
    m_playback->setExternalQueue(true);
    if(isOnline(current())){if(!m_onlineActive)m_playback->pauseForQueue();ensureOnlinePlayer();m_onlineActive=true;requestOnlineCurrent();}
    else{stopOnline();beginAttempt(current().id);m_playback->play({current().id},current().id);publish();}
}
void PlaybackFacade::skipUnavailableCurrent(const QString &message){
    const auto id=current().id;const auto operation=m_operation;
    if(!m_sharedQueueActive||!id||m_queueFailures.contains(id))return;
    m_queueFailures.insert(id);if(m_onlineActive)m_onlinePending=true;
    QTimer::singleShot(0,this,[this,id,operation,message]{
        if(isShutdown()||!m_sharedQueueActive||current().id!=id||m_operation!=operation||!m_queueFailures.contains(id))return;
        auto candidates=m_onlineQueue;
        // Shuffle may still have forward history before its next full bag.
        // Advance through it without retrying an item already known to fail.
        while(m_queueFailures.size()<candidates.items.size()){
            const auto next=candidates.next(true);if(!next)break;
            if(!m_queueFailures.contains(next)){m_onlineQueue=std::move(candidates);activateQueuedCurrent();return;}
        }
        const auto text=QStringLiteral("队列中没有可播放的后续内容")+(message.isEmpty()?QString():QStringLiteral("：")+message);
        stopUnavailableCurrent(text);
    });
}
void PlaybackFacade::stopUnavailableCurrent(const QString &message){
    m_onlineRetryTimer.stop();
    if(m_onlineActive){
        const auto resume=qMax<qint64>(0,position());finishOnlineSession();clearOnlineSource();
        m_preload.reset();m_dash.reset();m_onlineFader.reset();m_onlineResumePosition=resume;
        m_onlinePending=false;m_onlineFailed=true;m_onlineAutoPlay=false;
    }else m_playback->stopForQueue();
    emit playbackFailure(m_operation,current().id,current().title,message,-1);emit error(message);emit stateChanged();publish();
}
void PlaybackFacade::advanceShared(bool manual){
    if(m_onlineQueue.next(manual)){activateQueuedCurrent();return;}
    if(m_onlineActive){finishOnlineSession();m_onlineFader.reset();clearOnlineSource();m_onlinePending=false;}
    else m_playback->stopForQueue();
    emit stateChanged();publish();
}
void PlaybackFacade::playOnline(const QVector<Track> &tracks, qint64 start) {
    if(isShutdown()||tracks.isEmpty())return;
    // Keep the persisted local queue and listening session in the local engine.
    // Online IDs and expiring stream URLs live only in this session.
    if(!m_onlineActive){m_playback->pauseForQueue();m_playback->save();}
    ensureOnlinePlayer();
    finishOnlineSession();m_onlineFader.reset();clearOnlineSource();
    m_onlineTracks.clear();QVector<qint64> ids;
    for(const auto &track:tracks){if(track.id>0){m_onlineTracks.insert(track.id,track);ids.append(track.id);}}
    m_onlineQueue.mode=m_playback->mode();m_onlineQueue.set(ids,start);m_sharedQueueActive=!ids.isEmpty();m_onlineActive=m_sharedQueueActive;m_playback->setExternalQueue(m_sharedQueueActive);m_queueFailures.clear();
    if(m_onlineActive)requestOnlineCurrent();
}
void PlaybackFacade::resetOnlineBuffer(){
    if(isShutdown()||!m_onlinePlayer)return;
    m_onlineDecoded.clear();
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Normal playback uses the real audio output. A stable tap drives isolated
    // decode-only diagnostics without recreating Qt's output on every seek.
    if(m_onlineBufferOutput){m_onlinePlayer->setAudioBufferOutput(m_onlineBufferOutput.get());return;}
    if(!m_decodeOnly&&!qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))return;
    m_onlineBufferOutput = std::make_unique<QAudioBufferOutput>();
    connect(m_onlineBufferOutput.get(), &QAudioBufferOutput::audioBufferReceived,
            this, [this](const QAudioBuffer &buffer){
        if (isShutdown() || !m_onlinePlayer || !m_onlineActive || m_onlinePending || !buffer.isValid() || buffer.startTime() < 0) return;
        const auto start = buffer.startTime() / 1000;
        const auto end = (buffer.startTime() + buffer.duration()) / 1000;
        if (end <= start) return;
        // Only report decoded frames known to be available. Do not bridge gaps
        // or turn previously decoded, discarded audio into a pretend disk cache.
        if (!m_onlineDecoded.isEmpty() && start > m_onlineDecoded.latestTime() + 1)
            m_onlineDecoded.clear();
        m_onlineDecoded.addInterval(start, end);
        const auto position = m_onlinePlayer->position();
        if (position > 0) m_onlineDecoded.removeInterval(0, position - 1);
        notify(Notice::Snapshot);
    });
    m_onlinePlayer->setAudioBufferOutput(m_onlineBufferOutput.get());
#endif
}
void PlaybackFacade::ensureOnlinePlayer(){
    if(!m_onlinePlayer){
        m_onlinePlayer=std::make_unique<QMediaPlayer>(this);
        if(!m_onlineHealth.isValid()){
            m_onlineHealth.start();m_onlineRetryTimer.setSingleShot(true);
            connect(&m_onlineRetryTimer,&QTimer::timeout,this,[this]{if(m_onlineActive&&!isShutdown()&&m_onlinePending)requestOnlineCurrent(true,true,m_onlineAutoPlay);});
            m_onlineWatchdog.setInterval(1000);
            connect(&m_onlineWatchdog,&QTimer::timeout,this,[this]{
                if(m_onlineActive&&!m_onlinePending&&!m_onlineSeekWaiting&&!m_onlineFailed&&playing()&&m_onlineHealth.elapsed()>15000)
                    failOnlineSource(current().id,QStringLiteral("网络音源暂时无响应"),true);
            });m_onlineWatchdog.start();
        }
        if(!m_decodeOnly&&!qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS")){
            if(!m_onlineAudio)m_onlineAudio=std::make_unique<QAudioOutput>();
            m_onlineAudio->setDevice(m_playback->outputDevice());
            m_onlineAudio->setVolume(volume()*m_onlineFader.gain()/100.);
            m_onlinePlayer->setAudioOutput(m_onlineAudio.get());
        }
        const QPointer<QMediaPlayer> player=m_onlinePlayer.get();
        connect(player,&QMediaPlayer::sourceChanged,this,[this,player]{if(player&&player==m_onlinePlayer.get())resetOnlineBuffer();});
        connect(player,&QMediaPlayer::bufferProgressChanged,this,[this,player]{if(player&&player==m_onlinePlayer.get()&&m_onlineActive)notify(Notice::Snapshot);});
        connect(player,&QMediaPlayer::positionChanged,this,[this,player](qint64 p){if(player&&player==m_onlinePlayer.get()&&m_onlineActive){m_onlineHealth.restart();if(!m_onlinePending&&p>0&&m_onlineClock.elapsed()-m_onlineStableSince>=1000)m_queueFailures.clear();if(m_onlineRetryCount&&m_onlineClock.elapsed()-m_onlineStableSince>5000)m_onlineRetryCount=0;sampleOnline();notify(Notice::Position,p,duration());}});
        connect(player,&QMediaPlayer::durationChanged,this,[this,player](qint64 length){if(player&&player==m_onlinePlayer.get()&&m_onlineActive){
            auto track=current();if(length>0&&track.onlineMetadata.value("preview").toBool()){
                track.duration=length;track.onlineMetadata["previewDurationMs"]=length;updateOnlineTrack(track);
            }
            notify(Notice::Position,position(),duration());
        }});
        connect(player,&QMediaPlayer::seekableChanged,this,[this,player](bool){if(player&&player==m_onlinePlayer.get()&&m_onlineActive){applyOnlineResume();notify(Notice::Seekable);}});
        connect(player,&QMediaPlayer::playbackStateChanged,this,[this,player]{if(player&&player==m_onlinePlayer.get()&&m_onlineActive){m_onlineHealth.restart();if(playing()&&!m_onlineFader.pausing())m_onlineFader.to(1,m_library->setting("playbackFade",false).toBool());sampleOnline(false,true);syncOnlineVideo();notify(Notice::State);}});
        connect(player,&QMediaPlayer::mediaStatusChanged,this,[this,player](QMediaPlayer::MediaStatus status){
            if(!player||player!=m_onlinePlayer.get()||!m_onlineActive)return;
            if(status==QMediaPlayer::LoadedMedia||status==QMediaPlayer::BufferedMedia)applyOnlineResume();
            if(status==QMediaPlayer::EndOfMedia){
                // Seek/cancel notifications are not a natural track ending.
                if(m_onlinePending||m_onlineSeekWaiting||m_onlineResumePosition>=0||player->duration()<=0||player->position()+250<player->duration())return;
                sampleOnline(true,true);const auto id=current().id;const auto operation=m_operation,seekSerial=m_onlineSeekSerial;
                QTimer::singleShot(0,this,[this,player,id,operation,seekSerial]{
                    if(isShutdown()||!player||player!=m_onlinePlayer.get()||!m_onlineActive||m_onlinePending||m_onlineSeekWaiting||m_onlineResumePosition>=0||current().id!=id||m_operation!=operation||m_onlineSeekSerial!=seekSerial||player->mediaStatus()!=QMediaPlayer::EndOfMedia)return;
                    if(!selectOnlinePart(current().onlineMetadata.value("page").toInt(1)+1,true))advanceShared(false);
                });
            }
            else publish();
        });
        connect(player,&QMediaPlayer::errorOccurred,this,[this,player]{
            if(player&&player==m_onlinePlayer.get()&&m_onlineActive&&!m_onlinePending)failOnlineSource(current().id,QStringLiteral("在线音频播放失败，请重新选择歌曲或刷新登录状态"),true);
        });
    }
}
void PlaybackFacade::cancelOnlineStreams(){
    if(m_preload)m_preload->cancel();
    if(m_dash)m_dash->cancel();
}
void PlaybackFacade::clearOnlineSource(){
    ++m_onlineSeekSerial;
    if(m_onlinePlayer)m_onlinePlayer->disconnect(this);
    stopOnlineVideo();resetVideoFrame();cancelOnlineStreams();
    // A track change gets a fresh reader. Detach callbacks before cancelling
    // its source, so old EOF/error delivery cannot advance the new queue item.
    m_onlinePlayer.reset();
    if(!isShutdown())ensureOnlinePlayer();
}
void PlaybackFacade::requestOnlineCurrent(bool resume,bool retry,bool autoplay){
    if(!m_onlineActive||isShutdown())return;
    m_onlineRetryTimer.stop();m_onlinePending=true;m_onlineFailed=false;m_onlineAutoPlay=autoplay;
    if(!retry){finishOnlineSession();m_onlineRetryCount=0;}
    if(!resume){
        m_onlineResumePosition=-1;
        if(!retry&&current().hash.startsWith("bilibili:")){
            auto track=current();const auto pages=track.onlineMetadata.value("pages").toArray();
            if(!pages.isEmpty()){
                const auto first=pages.first().toObject();const auto cid=first.value("cid");
                track.onlineMetadata["cid"]=cid.isString()?cid.toString():QString::number(qint64(cid.toDouble()));track.onlineMetadata["page"]=first.value("page").toInt(1);
                track.title=first.value("part").toString(track.onlineMetadata.value("videoTitle").toString(track.title));
                const auto length=qint64(first.value("duration").toDouble()*1000);if(length>0)track.duration=length;updateOnlineTrack(track);
            }
        }
    }
    m_onlineFader.reset(m_library->setting("playbackFade",false).toBool()?0:1);
    clearOnlineSource();
    m_preload.reset();m_dash.reset();m_onlineDecoded.clear();m_onlinePending=true;beginAttempt(current().id);
    if(!retry)beginOnlineSession();
    emit trackChanged();emit stateChanged();publish();emit onlineSourceRequested(current().id);
}
void PlaybackFacade::setOnlineSource(qint64 id,const QUrl &url,const QMap<QByteArray,QByteArray> &headers,const QList<QUrl> &backups){
    if(!m_onlineActive||isShutdown()||current().id!=id||url.isEmpty())return;
    m_onlineRetryTimer.stop();m_onlinePending=true;m_onlineFailed=false;m_dash.reset();
    const auto playSource=[this](const QUrl &source){
        m_onlinePending=false;m_onlineFailed=false;m_onlineHealth.restart();m_onlineStableSince=m_onlineClock.elapsed();
        applyVideoSink();m_onlinePlayer->setSource(source);if(m_onlineAutoPlay)m_onlinePlayer->play();else m_onlinePlayer->pause();publish();
    };
    if(url.isLocalFile()){m_preload.reset();playSource(url);return;}
    m_preload=std::make_unique<MediaPreload>(DiskCache::online(m_library),duration());
    connect(m_preload.get(),&MediaPreload::ready,this,playSource);
    connect(m_preload.get(),&MediaPreload::rangesChanged,this,[this]{publish();});
    connect(m_preload.get(),&MediaPreload::failed,this,[this,id,headers,backups](const QString &message){
        if(!m_onlineActive||isShutdown()||current().id!=id)return;
        if(backups.isEmpty()){failOnlineSource(id,message,true);return;}
        // A CDN mirror may be unreachable even though the resolved video is
        // valid. Try the response's remaining mirrors before re-resolving it.
        m_onlineResumePosition=qMax<qint64>(0,position());m_onlinePending=true;
        clearOnlineSource();
        const auto operation=m_operation;publish();
        QTimer::singleShot(0,this,[this,id,operation,headers,backups]{
            if(!m_onlineActive||isShutdown()||current().id!=id||m_operation!=operation)return;
            setOnlineSource(id,backups.first(),headers,backups.mid(1));
        });
    });
    m_preload->start(url,headers);publish();
}
void PlaybackFacade::setOnlineVideoVisible(bool visible){
    if(m_onlineVideoVisible==visible)return;
    m_onlineVideoVisible=visible;
    if(visible&&m_videoSink&&wantsOnlineVideo())m_videoFrameReady=m_videoSink->videoFrame().isValid();
    m_onlineVideoTimer.start();
}
void PlaybackFacade::updateOnlineVideoMode(){
    if(isShutdown())return;
    if(!m_onlineActive||!current().hash.startsWith("bilibili:")){publish();return;}
    const bool video=wantsOnlineVideo();
    if(!m_dash){
        // Legacy combined files cannot stop video transfer independently. At
        // least avoid reloading their audio just because the view was hidden.
        if(m_preload){
            if(m_library->setting("onlineVideoMode",0).toInt()!=0)refreshOnlineSource();
            else if(m_onlinePlayer)m_onlinePlayer->setActiveVideoTrack(video?0:-1);
        }else if(!m_onlinePending&&m_onlineFailed&&video)refreshOnlineSource();
        publish();return;
    }
    if(m_dash->videoEnabled()==video){publish();return;}
    if(video&&!m_dash->hasVideo()){refreshOnlineSource();return;}
    if(!video)stopOnlineVideo(true);
    else{m_onlineVideoFailed=false;m_onlineVideoSync.start();}
    m_dash->setVideoEnabled(video);publish();
}
void PlaybackFacade::stopOnlineVideo(bool resumeAudio){
    m_onlineVideoSync.stop();
    if(m_dash)m_dash->closeVideoReaders();
    if(m_onlineVideoPlayer){m_onlineVideoPlayer->disconnect(this);m_onlineVideoPlayer.reset();}
    m_onlineVideoFrames.reset();m_onlineVideoAligned=false;m_onlineVideoAdjustment.invalidate();
    const bool resume=resumeAudio&&m_onlineSeekWaiting&&m_onlineSeekAutoplay;
    m_onlineSeekWaiting=false;m_onlineSeekClock.invalidate();resetVideoFrame();
    if(resume&&m_onlinePlayer&&!isShutdown()){m_onlineHealth.restart();m_onlinePlayer->play();}
}
void PlaybackFacade::startOnlineVideo(const QUrl &source){
    if(isShutdown()||!m_onlineActive||!m_dash||!wantsOnlineVideo())return;
    // No QAudioOutput is attached: there is only one audible player and clock.
    m_onlineVideoPlayer=std::make_unique<QMediaPlayer>();
    m_onlineVideoFrames=std::make_unique<QVideoSink>();
    m_onlineVideoAligned=false;m_onlineVideoAdjustment.invalidate();m_onlineVideoFailed=false;
    applyVideoSink();
    const QPointer<QMediaPlayer> player=m_onlineVideoPlayer.get();
    connect(m_onlineVideoFrames.get(),&QVideoSink::videoFrameChanged,this,[this,player](const QVideoFrame &frame){
        if(!player||player!=m_onlineVideoPlayer.get()||isShutdown()||!m_onlineActive||!m_onlinePlayer||!wantsOnlineVideo()||m_onlinePending||m_onlineResumePosition>=0||!frame.isValid())return;
        const auto target=m_onlineSeekWaiting?m_onlineSeekTarget:m_onlinePlayer->position();
        const auto stamp=frame.startTime()>=0?frame.startTime()/1000:player->position();
        // Never expose the old position/first frame while the picture catches up.
        if(qAbs(stamp-target)>150)return;
        m_onlineVideoAligned=true;
        if(m_videoSink)m_videoSink->setVideoFrame(frame);
        finishOnlineVideoSeek();
        if(m_onlineSeekWaiting||!playing())player->pause();
    });
    connect(player,&QMediaPlayer::mediaStatusChanged,this,[this,player](QMediaPlayer::MediaStatus status){
        if(!player||player!=m_onlineVideoPlayer.get())return;
        if(status==QMediaPlayer::LoadedMedia||status==QMediaPlayer::BufferedMedia)syncOnlineVideo();
    });
    connect(player,&QMediaPlayer::seekableChanged,this,[this,player]{if(player&&player==m_onlineVideoPlayer.get())syncOnlineVideo();});
    connect(player,&QMediaPlayer::errorOccurred,this,[this,player]{
        // Dispose a failed backend after its signal handler has unwound.
        QTimer::singleShot(0,this,[this,player]{if(player&&player==m_onlineVideoPlayer.get())failOnlineVideo(QStringLiteral("视频画面加载失败，继续播放音轨"));});
    });
    m_onlineVideoPlayer->setSource(source);m_onlineVideoSync.start();
}
void PlaybackFacade::syncOnlineVideo(bool force){
    if(isShutdown()||!m_onlineActive||!m_dash||!wantsOnlineVideo())return;
    if(m_onlineSeekWaiting&&m_onlineSeekClock.elapsed()>20000){failOnlineVideo(QStringLiteral("视频画面等待超时，继续播放音轨"));return;}
    if(!m_onlineVideoPlayer||m_onlinePending||m_onlineResumePosition>=0||!m_onlinePlayer||!m_onlineVideoPlayer->isSeekable())return;
    const auto audioStatus=m_onlinePlayer->mediaStatus();
    if(audioStatus==QMediaPlayer::NoMedia||audioStatus==QMediaPlayer::LoadingMedia||audioStatus==QMediaPlayer::InvalidMedia)return;
    finishOnlineVideoSeek();
    auto *video=m_onlineVideoPlayer.get();
    const auto target=m_onlineSeekWaiting?m_onlineSeekTarget:m_onlinePlayer->position();
    const auto drift=video->position()-target;
    // A seek needs time to decode its first frame. Do not restart it on every
    // audio position notification while the network is still supplying data.
    if(force||!m_onlineVideoAdjustment.isValid()||(qAbs(drift)>180&&m_onlineVideoAdjustment.elapsed()>(m_onlineVideoAligned?750:2000))){
        m_onlineVideoAligned=false;
        m_onlineVideoAdjustment.restart();video->setPlaybackRate(1);video->setPosition(target);
        video->play(); // A paused Qt reader may otherwise never deliver a seek frame.
    }else if(m_onlineVideoAligned){
        if(playing()&&!m_onlineSeekWaiting&&audioStatus!=QMediaPlayer::StalledMedia&&audioStatus!=QMediaPlayer::BufferingMedia){
            // Small drift is corrected by the silent picture, never the audio.
            video->setPlaybackRate(drift>50?.97:drift<-50?1.03:1.);
            if(video->playbackState()!=QMediaPlayer::PlayingState)video->play();
        }else video->pause();
    }
}
void PlaybackFacade::finishOnlineVideoSeek(){
    if(!m_onlineSeekWaiting||!m_onlineVideoAligned||!m_onlinePlayer||!m_onlineVideoFrames)return;
    const auto frame=m_onlineVideoFrames->videoFrame();
    if(!frame.isValid()||qAbs((frame.startTime()>=0?frame.startTime()/1000:m_onlineVideoPlayer->position())-m_onlineSeekTarget)>150)return;
    const auto status=m_onlinePlayer->mediaStatus();
    // A paused reader may stay LoadedMedia until play() is requested. Requiring
    // BufferedMedia here creates a wait-for-play / wait-for-buffer deadlock.
    if((status!=QMediaPlayer::LoadedMedia&&status!=QMediaPlayer::BufferedMedia)||!m_onlinePlayer->isSeekable()||qAbs(m_onlinePlayer->position()-m_onlineSeekTarget)>150)return;
    const bool autoplay=m_onlineSeekAutoplay;m_onlineSeekWaiting=false;m_onlineSeekClock.invalidate();m_onlineHealth.restart();
    if(autoplay)m_onlinePlayer->play();else m_onlineVideoPlayer->pause();
    publish();
}
void PlaybackFacade::failOnlineVideo(const QString &message){
    if(isShutdown()||!m_onlineActive||!m_dash||!wantsOnlineVideo())return;
    stopOnlineVideo(true);m_onlineVideoFailed=true;m_dash->setVideoEnabled(false);
    emit error(message);publish();
}
void PlaybackFacade::refreshOnlineSource(){
    if(!m_onlineActive||isShutdown())return;
    // A second quality change can arrive while the first source is loading.
    // Keep the user's transport intent and resume point, not the temporary
    // stopped/zero position of the player being replaced.
    const bool autoplay=m_onlinePending?m_onlineAutoPlay:playing();
    if(!m_onlinePending||m_onlineResumePosition<0)m_onlineResumePosition=position();
    m_onlineRetryCount=0;requestOnlineCurrent(true,true,autoplay);
}
bool PlaybackFacade::selectOnlinePart(int page,bool automatic){
    if(isShutdown()||!m_onlineActive||!current().hash.startsWith("bilibili:"))return false;
    auto track=current();const auto parts=track.onlineMetadata.value("pages").toArray();QJsonObject selected;
    for(const auto &value:parts)if(value.toObject().value("page").toInt()==page){selected=value.toObject();break;}
    const auto identifier=[](const QJsonValue &value){return value.isString()?value.toString():QString::number(qint64(value.toDouble()));};
    const auto cid=identifier(selected.value("cid")),oldCid=track.onlineMetadata.value("cid").toString();
    if(selected.isEmpty()||cid.isEmpty()||cid=="0"||cid==oldCid)return false;
    const bool autoplay=automatic|| (m_onlinePending?m_onlineAutoPlay:playing());
    auto positions=track.onlineMetadata.value("partProgress").toObject();
    if(!oldCid.isEmpty())positions[oldCid]=m_onlinePlayer->mediaStatus()==QMediaPlayer::EndOfMedia?0:double(position());
    const auto videoTitle=track.onlineMetadata.value("videoTitle").toString(track.title);
    const auto length=qint64(selected.value("duration").toDouble()*1000);
    auto resume=automatic?0:qMax<qint64>(0,qint64(positions.value(cid).toDouble()));if(length>0&&resume>=length)resume=0;
    finishOnlineSession();track.onlineMetadata["partProgress"]=positions;track.onlineMetadata["videoTitle"]=videoTitle;
    track.onlineMetadata["cid"]=cid;track.onlineMetadata["page"]=page;
    track.title=parts.size()>1?selected.value("part").toString(videoTitle):videoTitle;track.duration=length;
    m_onlineResumePosition=resume;m_queueFailures.clear();updateOnlineTrack(track);
    requestOnlineCurrent(true,false,autoplay);return true;
}
void PlaybackFacade::setOnlineDashSource(qint64 id,const QJsonObject &video,const QJsonObject &audio,const QMap<QByteArray,QByteArray> &headers){
    if(!m_onlineActive||isShutdown()||current().id!=id)return;
    m_onlineRetryTimer.stop();m_onlinePending=true;m_onlineFailed=false;m_preload.reset();
    m_dash=std::make_unique<DashStream>(DiskCache::online(m_library),duration(),video,audio,headers);
    m_dash->setVideoEnabled(wantsOnlineVideo());
    connect(m_dash.get(),&DashStream::ready,this,[this](const QUrl &source){
        m_onlinePending=false;m_onlineFailed=false;m_onlineHealth.restart();m_onlineStableSince=m_onlineClock.elapsed();
        applyVideoSink();m_onlinePlayer->setSource(source);if(m_onlineAutoPlay)m_onlinePlayer->play();else m_onlinePlayer->pause();publish();
    });
    connect(m_dash.get(),&DashStream::videoReady,this,&PlaybackFacade::startOnlineVideo);
    connect(m_dash.get(),&DashStream::videoFailed,this,[this](const QString &message){
        const QPointer<DashStream> stream=m_dash.get();
        QTimer::singleShot(0,this,[this,stream,message]{if(stream&&stream==m_dash.get())failOnlineVideo(message);});
    });
    connect(m_dash.get(),&DashStream::rangesChanged,this,[this]{publish();});
    connect(m_dash.get(),&DashStream::failed,this,[this,id](const QString &message){failOnlineSource(id,message,true);});
    m_onlineVideoFailed=false;
    if(wantsOnlineVideo())m_onlineVideoSync.start();
    m_dash->start();publish();
}
void PlaybackFacade::applyOnlineResume(){
    if(isShutdown()||!m_onlinePlayer||!m_onlineActive||m_onlineResumePosition<0||!m_onlinePlayer->isSeekable())return;
    const auto status=m_onlinePlayer->mediaStatus();
    if(status!=QMediaPlayer::LoadedMedia&&status!=QMediaPlayer::BufferedMedia)return;
    const auto target=m_onlinePlayer->duration()>0?qMin(m_onlineResumePosition,m_onlinePlayer->duration()-1):m_onlineResumePosition;
    resetOnlineBuffer();m_onlinePlayer->setPosition(target);m_onlineLastPosition=target;m_onlineLastWall=m_onlineClock.elapsed();m_onlineResumePosition=-1;
}
void PlaybackFacade::updateOnlineTrack(const Track &track){
    if(!m_onlineTracks.contains(track.id))return;
    m_onlineTracks.insert(track.id,track);
    if(m_onlineSession.id&&m_onlineHistoryTrack.id==track.id){
        m_onlineHistoryTrack=track;
        const auto full=track.onlineMetadata.value("fullDurationMs").toInteger();
        if(track.onlineMetadata.value("preview").toBool()){m_onlineSession.sought=true;if(full>0)m_onlineSession.duration=full;}
    }
    emit onlineTrackUpdated(track.id);
    if(m_onlineActive)publish();
}
void PlaybackFacade::failOnlineSource(qint64 id,const QString &message,bool transient){
    if(!m_onlineActive||isShutdown()||current().id!=id||m_onlineRetryTimer.isActive())return;
    const auto resume=qMax<qint64>(0,position());
    m_onlineDecoded.clear();m_onlineFailed=true;
    if(transient&&m_onlineRetryCount<2){
        ++m_onlineRetryCount;m_onlinePending=true;m_onlineResumePosition=resume;
        m_onlineRetryTimer.start(600*m_onlineRetryCount);publish();return;
    }
    // A Qishui account/protocol/codec failure can affect every item in a list.
    // Keep the selected track and the actual failure instead of scanning it all.
    if(current().hash.startsWith("qishui:")){stopUnavailableCurrent(message);return;}
    skipUnavailableCurrent(message);
}
void PlaybackFacade::stopOnline(){
    if(!m_onlineActive)return;
    finishOnlineSession();
    m_onlineRetryTimer.stop();m_onlineDecoded.clear();m_onlineActive=false;m_onlinePending=false;m_onlineFailed=false;
    m_onlineResumePosition=-1;m_onlineFader.reset();
    clearOnlineSource();
    m_preload.reset();m_dash.reset();emit trackChanged();emit stateChanged();publish();
}
void PlaybackFacade::beginOnlineSession(){
    auto track=current();
    auto matchTrack=track;const auto full=track.onlineMetadata.value("fullDurationMs").toInteger();
    if(track.onlineMetadata.value("preview").toBool()&&full>0)matchTrack.duration=full;
    if(!m_matchCandidatesReady){m_matchCandidates=m_library->tracks();m_matchCandidatesReady=true;}
    if(!m_onlineMatches.contains(track.hash))m_onlineMatches.insert(track.hash,matchOnlineTrack(matchTrack,m_matchCandidates));
    track.matchedLocalId=m_onlineMatches.value(track.hash);m_onlineTracks.insert(track.id,track);
    m_onlineHistoryTrack=track;m_onlineHistoryHeard=0;
    m_onlineSessionMatched=track.matchedLocalId>0;m_onlineThreshold=qBound(70,m_library->setting("threshold",80).toInt(),100);
    matchTrack.matchedLocalId=track.matchedLocalId;
    m_onlineSession=m_onlineSessionMatched?m_library->startSession(track.matchedLocalId,matchTrack.duration):m_library->startOnlineSession(matchTrack);
    if(track.onlineMetadata.value("preview").toBool())m_onlineSession.sought=true;
    if(m_onlineResumePosition>0)m_onlineSession.sought=true;
    m_onlineLastWall=m_onlineClock.elapsed();m_onlineLastPosition=0;
}
void PlaybackFacade::sampleOnline(bool finishing,bool flush){
    if(!m_onlineSession.id||!m_onlinePlayer)return;
    const auto now=m_onlineClock.elapsed(),pos=m_onlinePlayer->position();qint64 delta=pos-m_onlineLastPosition;
    if(!m_onlinePending&&m_onlineResumePosition<0&&(playing()||finishing||flush)&&delta>0)delta=qMin(delta,qMax<qint64>(0,now-m_onlineLastWall)+80);else delta=0;
    if(m_onlineSession.duration<=0&&m_onlinePlayer->duration()>0)m_onlineSession.duration=m_onlinePlayer->duration();
    if(delta>0||finishing){const bool ok=m_onlineSessionMatched?m_library->saveSession(m_onlineSession,delta,m_onlineThreshold,finishing):m_library->saveOnlineSession(m_onlineSession,delta,m_onlineThreshold,finishing);if(!ok)emit saveFailed("在线收听统计未能保存");else m_onlineHistoryHeard+=delta;}
    if(pos!=m_onlineLastPosition){m_onlineLastPosition=pos;m_onlineLastWall=now;}
}
void PlaybackFacade::finishOnlineSession(){if(!m_onlineSession.id)return;sampleOnline(m_onlinePlayer&&m_onlinePlayer->mediaStatus()==QMediaPlayer::EndOfMedia,true);if(m_onlineHistoryHeard>0&&!m_library->recordPlaybackHistory(m_onlineHistoryTrack))emit saveFailed("播放历史未能保存");m_onlineHistoryHeard=0;m_onlineSession={};}
bool PlaybackFacade::saveOnlineState(){
    QJsonObject saved;
    if(m_sharedQueueActive&&m_library->setting("rememberPosition",true).toBool()){
        QJsonArray tracks;
        for(auto id:m_onlineQueue.items){const auto track=this->track(id);QJsonObject metadata;
            for(const auto &key:QStringList{"id","mid","mediaMid","songType","albumId","audioId","bvid","cid","page","videoTitle","partProgress","cover","thumbnail","provider","preview","previewStartMs","previewDurationMs","fullDurationMs"})if(track.onlineMetadata.contains(key))metadata[key]=track.onlineMetadata.value(key);
            if(track.hash.startsWith("bilibili:")&&id==current().id){const auto cid=metadata.value("cid").toString();if(!cid.isEmpty()){auto positions=metadata.value("partProgress").toObject();positions[cid]=double(position());metadata["partProgress"]=positions;}}
            tracks.append(QJsonObject{{"id",QString::number(id)},{"hash",track.hash},{"title",track.title},{"artist",track.artist},{"album",track.album},{"duration",double(track.duration)},{"cover",track.cover},{"vip",track.requiresSubscription},{"metadata",metadata}});
        }
        saved={{"tracks",tracks},{"current",QString::number(m_onlineQueue.current())},{"position",double(position())}};
    }
    if(saved==m_savedOnline&&m_library->setting("onlinePlayback").toObject()==saved){m_onlineSaveError.clear();return true;}
    const bool ok=m_library->setSetting("onlinePlayback",saved);if(ok){m_savedOnline=saved;m_onlineSaveError.clear();}else{m_onlineSaveError="在线播放进度未能保存："+m_library->lastError();emit saveFailed(m_onlineSaveError);}return ok;
}
}
