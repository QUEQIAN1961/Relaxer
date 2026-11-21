#include "playback-facade.h"
#include <QVideoFrame>
#include <QJsonArray>
#include <algorithm>

namespace relaxer {
PlaybackFacade::PlaybackFacade(Library *library, QObject *parent, bool decodeOnly)
    : QObject(parent), m_playback(new Playback(library, this, decodeOnly)), m_library(library), m_decodeOnly(decodeOnly), m_onlineFader([this](qreal gain){if(m_onlineAudio)m_onlineAudio->setVolume(volume()*gain/100.);}), m_saveTimer(this) {
    Q_ASSERT(library && library->thread() == thread());
    if(!decodeOnly&&!qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS")){
        m_audioDevices=std::make_unique<QMediaDevices>();
        connect(m_audioDevices.get(),&QMediaDevices::audioOutputsChanged,this,[this]{
            if(isShutdown())return;
            const auto device=m_playback->outputDevice();
            m_playback->applyOutputDevice(device);
            if(m_onlineAudio)m_onlineAudio->setDevice(device);
            emit audioOutputsChanged();
        });
    }
    connect(library,&Library::settingsUpdated,this,[this](const QStringList &keys){if(keys.contains("playbackFade")&&!m_library->setting("playbackFade",false).toBool())m_onlineFader.finish();});
    connect(library,&Library::settingsUpdated,this,[this](const QStringList &keys){if(keys.contains("localVideoMode")&&!m_onlineActive&&isVideoTrack(current())){resetVideoFrame();if(!isShutdown())publish();}});
    m_onlineVideoTimer.setSingleShot(true);m_onlineVideoTimer.setInterval(0);
    connect(&m_onlineVideoTimer,&QTimer::timeout,this,&PlaybackFacade::updateOnlineVideoMode);
    m_onlineVideoSync.setInterval(100);
    connect(&m_onlineVideoSync,&QTimer::timeout,this,[this]{syncOnlineVideo();});
    connect(library,&Library::settingsUpdated,this,[this](const QStringList &keys){if(keys.contains("onlineVideoMode"))m_onlineVideoTimer.start();});
    m_onlineClock.start();connect(library,&Library::changed,this,[this]{m_matchCandidatesReady=false;m_matchIndex.reset();m_onlineMatches.clear();++m_onlineMetadataRevision;});
    connect(m_playback, &Playback::trackChanged, this, [this] { if(!m_onlineActive){resetVideoFrame();notify(Notice::Track);} });
    connect(m_playback, &Playback::stateChanged, this, [this] { if(!m_onlineActive)notify(Notice::State); });
    connect(m_playback, &Playback::positionChanged, this,
            [this](qint64 pos, qint64 duration) { if(!m_onlineActive){if(pos>0&&playing())m_queueFailures.clear();notify(Notice::Position, pos, duration);} });
    connect(m_playback, &Playback::error, this,
            [this](const QString &message) { if(!m_onlineActive)notify(Notice::Error, 0, 0, message); });
    connect(m_playback, &Playback::saveFailed, this, &PlaybackFacade::saveFailed, Qt::QueuedConnection);
    connect(m_playback,&Playback::advanceRequested,this,[this](bool failed){
        if(!m_sharedQueueActive||m_onlineActive)return;
        if(failed){skipUnavailableCurrent(QStringLiteral("本地媒体无法播放"));return;}
        advanceShared(false);
    });
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    m_onlineBufferOutput.reset();
#endif
    m_onlinePlayer.reset();m_onlineAudio.reset();
    // Release media/process children before QObject deletes other facade children.
    delete m_playback;
}
PlaybackSnapshot PlaybackFacade::snapshot() const {
    PlaybackSnapshot value;
    value.revision = m_snapshotSerial;
    value.track = current();
    value.position = position();
    value.duration = duration();
    value.buffered = bufferedTimeRanges();
    value.playing = playing();
    value.video = isVideoTrack(value.track);value.videoFrameReady=m_videoFrameReady;value.onlineVideoVisible=m_onlineVideoVisible;
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
QMediaTimeRange PlaybackFacade::bufferedTimeRanges() const {
    const auto length = duration();
    if (isShutdown() || !current().id || length <= 0) return {};
    if (!m_onlineActive) {
        // Local audio and MIDI are fully readable; they need no network preload.
        return m_playback->seekable() && m_playback->media()->error() == QMediaPlayer::NoError
            ? QMediaTimeRange(0, length) : QMediaTimeRange{};
    }
    if (m_onlinePending || !m_onlinePlayer || m_onlinePlayer->source().isEmpty() ||
        m_onlineFailed || m_onlinePlayer->error() != QMediaPlayer::NoError) return {};
    const auto status = m_onlinePlayer->mediaStatus();
    if (status == QMediaPlayer::NoMedia || status == QMediaPlayer::LoadingMedia ||
        status == QMediaPlayer::InvalidMedia) return {};
    if (m_onlinePlayer->source().isLocalFile()) return QMediaTimeRange(0, length);
    if (m_preload && m_preload->complete()) return QMediaTimeRange(0, length);
    // bufferProgress() is the fullness of the playback buffer, NOT a fraction
    // of the song. In Qt's FFmpeg backend it is currently just 0, .25 or 1.
    auto ranges = m_dash?m_dash->ranges():m_preload ? m_preload->ranges() : QMediaTimeRange{};
    if (ranges.isEmpty()) ranges = m_onlinePlayer->bufferedTimeRange();
    if (ranges.isEmpty()) ranges = m_onlineDecoded;
    QMediaTimeRange visible;
    for (const auto &range : ranges.intervals()) {
        const auto start = qBound<qint64>(0, range.start(), length);
        const auto end = qBound<qint64>(0, range.end(), length);
        if (end > start) visible.addInterval(start, end);
    }
    return visible;
}
void PlaybackFacade::notify(Notice notice, qint64 pos, qint64 duration, const QString &message) {
    if (isShutdown())
        return;
    const auto serial=++m_snapshotSerial;
    const auto value = snapshot();
    if (notice == Notice::State && value.playing && m_failedTrack) {
        m_failedTrack = 0;
        emit playbackRecovered();
    }
    // New value notifications run after the engine unwinds. Keep the original
    // UI signal timing: animations and immediate error toasts depend on it.
    QMetaObject::invokeMethod(
        this,
        [this, value, serial] {
            if (!isShutdown()&&serial==m_snapshotSerial)
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
void PlaybackFacade::setVideoSink(QVideoSink *sink){
    if(m_videoSink==sink)return;disconnect(m_videoFrameConnection);m_videoSink=sink;m_videoFrameReady=false;
    if(sink)m_videoFrameConnection=connect(sink,&QVideoSink::videoFrameChanged,this,[this]{
        const bool ready=m_videoSink&&m_videoSink->videoFrame().isValid()&&(!m_onlineActive||wantsOnlineVideo());
        if(m_videoFrameReady!=ready){m_videoFrameReady=ready;if(!isShutdown())publish();}
    });
    applyVideoSink();
    if(m_onlineActive&&m_dash&&m_onlineVideoAligned&&m_onlineVideoFrames&&sink)sink->setVideoFrame(m_onlineVideoFrames->videoFrame());
    if(!isShutdown())publish();
}
void PlaybackFacade::resetVideoFrame(){m_videoFrameReady=false;if(m_videoSink)m_videoSink->setVideoFrame({});}
void PlaybackFacade::applyVideoSink(){
    const bool separate=m_onlineActive&&bool(m_dash);
    auto *active=m_onlineActive?m_onlinePlayer.get():m_playback->media();
    auto *inactive=m_onlineActive?m_playback->media():m_onlinePlayer.get();
    if(inactive&&inactive->videoSink())inactive->setVideoSink(nullptr);
    auto *target=separate?nullptr:m_videoSink.data();
    if(active&&active->videoSink()!=target)active->setVideoSink(target);
    if(separate&&m_onlineVideoPlayer&&m_onlineVideoFrames){
        auto *rhi=m_videoSink?m_videoSink->rhi():nullptr;
        if(m_onlineVideoFrames->rhi()!=rhi)m_onlineVideoFrames->setRhi(rhi);
        if(m_onlineVideoPlayer->videoSink()!=m_onlineVideoFrames.get())m_onlineVideoPlayer->setVideoSink(m_onlineVideoFrames.get());
    }
}
void PlaybackFacade::publish() {
    ++m_snapshotSerial;applyVideoSink();
    if (!isShutdown())
        emit snapshotChanged(snapshot());
}
QVector<qint64> PlaybackFacade::upcoming() {
    return isShutdown() ? QVector<qint64>{} : m_sharedQueueActive ? m_onlineQueue.upcoming() : m_playback->upcoming();
}
void PlaybackFacade::beginAttempt(qint64 requested) {
    ++m_operation; m_requestedTrack = requested; m_reportedFailures.clear();
    emit playbackAttemptStarted(m_operation);
}
void PlaybackFacade::play(const QVector<qint64> &ids, qint64 start) {
    if (!isShutdown()) {
        if(m_sharedQueueActive&&std::any_of(ids.cbegin(),ids.cend(),[this](qint64 id){return isOnline(track(id));})){m_queueFailures.clear();m_onlineQueue.set(ids,start);activateQueuedCurrent();return;}
        stopOnline();
        m_sharedQueueActive=false;pruneOnlineTracks();m_playback->setExternalQueue(false);m_queueFailures.clear();
        beginAttempt(start ? start : ids.value(0));
        m_playback->play(ids, start);
        m_requestedTrack = 0;
        publish();
    }
}
void PlaybackFacade::toggle() {
    if (!isShutdown()) {
        m_queueFailures.clear();
        if(m_onlineActive){
            if(m_onlineSeekWaiting){m_onlineSeekAutoplay=!m_onlineSeekAutoplay;m_onlineAutoPlay=m_onlineSeekAutoplay;publish();return;}
            if(m_onlinePending||m_onlineFailed||m_onlinePlayer->source().isEmpty()||m_onlinePlayer->mediaStatus()==QMediaPlayer::EndOfMedia||m_onlinePlayer->mediaStatus()==QMediaPlayer::InvalidMedia)requestOnlineCurrent(m_onlineResumePosition>=0);
            else if(playing()&&!m_onlineFader.pausing())m_onlineFader.to(0,m_library->setting("playbackFade",false).toBool(),[this]{m_onlinePlayer->pause();});
            else{if(!playing())m_onlineFader.reset(m_library->setting("playbackFade",false).toBool()?0:1);else m_onlineFader.to(1,m_library->setting("playbackFade",false).toBool());m_onlinePlayer->play();}
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
        if(m_sharedQueueActive){m_queueFailures.clear();advanceShared(true);return;}
        beginAttempt();
        m_playback->next();
        publish();
    }
}
void PlaybackFacade::previous() {
    if (!isShutdown()) {
        if(m_sharedQueueActive){m_queueFailures.clear();if(m_onlineQueue.previous())activateQueuedCurrent();return;}
        beginAttempt();
        m_playback->previous();
        publish();
    }
}
void PlaybackFacade::playUpcoming(int index) {
    if (!isShutdown()) {
        if(m_sharedQueueActive){const auto list=m_onlineQueue.upcoming();if(index<0||index>=list.size())return;m_queueFailures.clear();for(int i=0;i<=index;++i)m_onlineQueue.next(true);activateQueuedCurrent();return;}
        beginAttempt();
        m_playback->playUpcoming(index);
        publish();
    }
}
void PlaybackFacade::seek(qint64 pos) {
    if (!isShutdown()) {
        if(m_onlineActive){
            if(seekable()){
                sampleOnline(false,true);m_onlineSession.sought=true;
                if(m_onlineSession.id){if(m_onlineSessionMatched)m_library->saveSession(m_onlineSession,0,m_onlineThreshold);else m_library->saveOnlineSession(m_onlineSession,0,m_onlineThreshold);}
                const auto target=qBound<qint64>(0,pos,qMax<qint64>(0,duration()-1));
                ++m_onlineSeekSerial;
                if(m_dash&&wantsOnlineVideo()&&m_dash->hasVideo()&&!m_onlineVideoFailed){
                    m_onlineSeekAutoplay=playing()&&!m_onlineFader.pausing();m_onlineSeekWaiting=true;m_onlineSeekTarget=target;m_onlineSeekClock.start();
                    m_onlineAutoPlay=m_onlineSeekAutoplay;
                    m_onlineFader.reset(m_onlineFader.pausing()?0:m_onlineFader.gain());
                    // Keep the last displayed frame while the new seek buffers.
                    // A view that has not shown a frame yet keeps its cover.
                    m_onlineVideoAligned=false;
                    if(m_onlineVideoPlayer)m_onlineVideoPlayer->pause();
                    m_onlinePlayer->pause();
                }
                m_onlineLastPosition=target;m_onlineLastWall=m_onlineClock.elapsed();resetOnlineBuffer();m_onlineHealth.restart();m_onlinePlayer->setPosition(target);
                syncOnlineVideo(true);
            }
            publish();return;
        }
        m_playback->seek(pos);
        publish();
    }
}
void PlaybackFacade::setMode(Mode mode) {
    if(m_sharedQueueActive)m_onlineQueue.setMode(mode);
    if (!isShutdown())
        m_playback->setMode(mode);
}
void PlaybackFacade::setVolume(int volume) {
    if(m_onlineAudio)m_onlineAudio->setVolume(qBound(0,volume,100)*m_onlineFader.gain()/100.);
    if (!isShutdown()) {
        m_playback->setVolume(volume);
        publish();
    }
}
QVariantList PlaybackFacade::audioOutputs() const {
    if(!m_audioDevices)return {};
    const auto devices=QMediaDevices::audioOutputs();const auto fallback=QMediaDevices::defaultAudioOutput();
    const auto saved=m_library->setting("audioOutputDevice",QString()).toString();
    bool found=false;for(const auto &device:devices)if(QString::fromLatin1(device.id().toBase64())==saved)found=true;
    QVariantList rows{QVariantMap{{"id",QString()},{"name",QStringLiteral("跟随系统默认")+(fallback.isNull()?QString():QStringLiteral(" · ")+fallback.description())},{"selected",!found},{"available",!fallback.isNull()}}};
    for(const auto &device:devices)rows.append(QVariantMap{{"id",QString::fromLatin1(device.id().toBase64())},{"name",device.description()},{"selected",QString::fromLatin1(device.id().toBase64())==saved},{"available",true}});
    return rows;
}
bool PlaybackFacade::selectAudioOutput(const QString &id){
    if(isShutdown()||!m_audioDevices)return false;
    auto device=QMediaDevices::defaultAudioOutput();
    if(!id.isEmpty()){
        device={};for(const auto &entry:QMediaDevices::audioOutputs())if(QString::fromLatin1(entry.id().toBase64())==id){device=entry;break;}
    }
    if(device.isNull()){emit error(QStringLiteral("音频输出设备已断开，请重新选择"));emit audioOutputsChanged();return false;}
    const auto previous=m_playback->outputDevice();
    if(!m_playback->applyOutputDevice(device))return false;
    if(!m_library->setSetting("audioOutputDevice",id)){
        m_playback->applyOutputDevice(previous);emit saveFailed(m_library->lastError());return false;
    }
    if(m_onlineAudio)m_onlineAudio->setDevice(device);
    emit audioOutputsChanged();return true;
}
void PlaybackFacade::pruneOnlineTracks(){
    const QSet<qint64> retained(m_onlineQueue.items.cbegin(),m_onlineQueue.items.cend());
    for(auto it=m_onlineTracks.begin();it!=m_onlineTracks.end();)
        if(!m_sharedQueueActive||!retained.contains(it.key()))it=m_onlineTracks.erase(it);else ++it;
    if(!m_sharedQueueActive){m_onlineQueue.set({},0);m_matchIndex.reset();m_matchCandidatesReady=false;m_onlineMatches.clear();}
    ++m_onlineMetadataRevision;
}
void PlaybackFacade::forget(const QVector<qint64> &ids) {
    if(m_sharedQueueActive){
        auto remaining=m_onlineQueue.items;const auto before=m_onlineQueue.current();for(auto id:ids)remaining.removeAll(id);
        m_onlineQueue.set(remaining,remaining.contains(before)?before:0);
        if(remaining.isEmpty()){stopOnline();m_sharedQueueActive=false;m_playback->setExternalQueue(false);m_playback->play({});emit trackChanged();publish();}
        else if(before!=m_onlineQueue.current())activateQueuedCurrent();
        else{emit stateChanged();publish();}
        pruneOnlineTracks();return;
    }
    if (!isShutdown())
        m_playback->forget(ids);
}
void PlaybackFacade::restore(bool includeOnline) {
    if (!isShutdown()) {
        beginAttempt();
        m_playback->restore();
        auto saved=m_library->setting("onlinePlayback").toObject();
        const auto progress=m_library->setting("onlinePlaybackProgress").toObject();
        if(!saved.value("revision").toString().isEmpty()&&progress.value("revision")==saved.value("revision")){
            saved["current"]=progress.value("current");saved["position"]=progress.value("position");
            auto rows=saved.value("tracks").toArray();
            for(int i=0;i<rows.size();++i){auto row=rows[i].toObject();if(row.value("id")==progress.value("current")){
                auto metadata=row.value("metadata").toObject();const auto cid=metadata.value("cid").toString();
                if(!cid.isEmpty()){auto positions=metadata.value("partProgress").toObject();positions[cid]=progress.value("position");metadata["partProgress"]=positions;row["metadata"]=metadata;rows[i]=row;}
            }}saved["tracks"]=rows;
        }
        if(includeOnline&&m_library->setting("rememberPosition",true).toBool()&&!saved.isEmpty()){
            QVector<qint64> ids;
            for(const auto &entry:saved.value("tracks").toArray()){
                const auto row=entry.toObject();Track track;track.id=row.value("id").toString().toLongLong();track.hash=row.value("hash").toString();
                if(track.id<=0)continue;
                if(!isOnline(track)){if(m_library->track(track.id).id)ids.append(track.id);continue;}
                track.title=row.value("title").toString();track.artist=row.value("artist").toString();track.album=row.value("album").toString();track.duration=qint64(row.value("duration").toDouble());track.cover=row.value("cover").toString();track.onlineMetadata=row.value("metadata").toObject();track.requiresSubscription=row.value("vip").toBool();m_onlineTracks.insert(track.id,track);ids.append(track.id);
            }
            if(!ids.isEmpty()){
                m_onlineQueue.mode=m_playback->mode();m_onlineQueue.set(ids,saved.value("current").toString().toLongLong());m_sharedQueueActive=true;m_playback->setExternalQueue(true);
                const auto position=qMax<qint64>(0,qint64(saved.value("position").toDouble()));
                m_onlineActive=isOnline(current());if(m_onlineActive){ensureOnlinePlayer();m_onlineResumePosition=position;}else m_playback->restoreQueued(current().id,position);
                emit trackChanged();
            }
        }
        publish();
    }
}
bool PlaybackFacade::save() {
    if(m_onlineActive)sampleOnline(false,true);
    if (isShutdown())
        return m_playback->shutdown();
    const bool onlineOk=saveOnlineState();
    const bool ok = m_playback->save()&&onlineOk;
    if(!ok||m_playback->lastSaveWrote())emit saved(ok);
    return ok;
}
bool PlaybackFacade::shutdown(bool force) {
    if (isShutdown() || m_shuttingDown)
        return m_playback->isShutdown()?m_playback->shutdown():false;
    m_shuttingDown = true;
    // A failed online save must still notify subscribers: the exit dialog and
    // the persistence tests drive their cancel/retry contract from saved(false),
    // not only from the boolean return value.
    if(!saveOnlineState()&&!force){m_shuttingDown=false;emit saved(false);return false;}
    finishOnlineSession();
    const bool ok = m_playback->shutdown(force);
    if(isShutdown()){m_saveTimer.stop();m_onlineRetryTimer.stop();m_onlineWatchdog.stop();clearOnlineSource();m_preload.reset();m_dash.reset();m_onlineFader.reset();}
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
