#pragma once
#include "playback.h"
#include "media-preload.h"
#include "dash-stream.h"
#include "online-track-matcher.h"
#include <QVideoSink>
#include <QSet>
#include <QElapsedTimer>
#include <QMediaTimeRange>
#include <QMediaDevices>
#include <QVariantList>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QAudioBufferOutput>
#endif

namespace relaxer {
// A value read from the authoritative engine. Reading never forecasts shuffle bags.
struct PlaybackSnapshot {
    quint64 revision = 0;
    Track track;
    qint64 position = 0, duration = 0;
    QMediaTimeRange buffered;
    bool playing = false, seekable = false, shutdown = false, video = false,videoFrameReady=false,onlineVideoVisible=true;
    int volume = 0;
    Mode mode = Mode::ListLoop;
    QMediaPlayer::PlaybackState transport = QMediaPlayer::StoppedState;
    QMediaPlayer::MediaStatus mediaStatus = QMediaPlayer::NoMedia;
    QVector<qint64> queue;
};
class PlaybackFacade final : public QObject {
    Q_OBJECT
  public:
    explicit PlaybackFacade(Library *library, QObject *parent = nullptr, bool decodeOnly = false);
    ~PlaybackFacade() override;
    PlaybackSnapshot snapshot() const;
    quint64 snapshotRevision() const { return m_snapshotSerial; }
    // Compatibility seam for existing consumers; production UI uses the facade.
    Playback *backend() const { return m_playback; }
    Track current() const { return m_sharedQueueActive ? track(m_onlineQueue.current()) : m_playback->current(); }
    Track track(qint64 id) const { return m_onlineTracks.contains(id) ? m_onlineTracks.value(id) : m_library->track(id); }
    bool onlineActive() const { return m_onlineActive; }
    qint64 position() const { return m_onlineActive ? (m_onlineResumePosition>=0?m_onlineResumePosition:m_onlinePlayer ? m_onlinePlayer->position() : 0) : m_playback->position(); }
    qint64 duration() const { return m_onlineActive ? (m_onlinePlayer && m_onlinePlayer->duration() > 0 ? m_onlinePlayer->duration() : current().duration) : m_playback->duration(); }
    bool playing() const { return m_onlineActive ? (m_onlineSeekWaiting ? m_onlineSeekAutoplay : m_onlinePlayer && m_onlinePlayer->playbackState() == QMediaPlayer::PlayingState) : m_playback->playing(); }
    bool seekable() const { return m_onlineActive ? !m_onlinePending && m_onlineResumePosition<0 && m_onlinePlayer && m_onlinePlayer->isSeekable() : m_playback->seekable(); }
    int volume() const { return m_playback->volume(); }
    Mode mode() const { return m_sharedQueueActive ? m_onlineQueue.mode : m_playback->mode(); }
    const Queue &queue() const { return m_sharedQueueActive ? m_onlineQueue : m_playback->queue(); }
    QString roundStatus() const {
        if(!m_onlineActive)return m_playback->roundStatus();
        const auto hash=current().hash;
        return (hash.startsWith("qqmusic:")?QStringLiteral("QQ 音乐"):hash.startsWith("kugou:")?QStringLiteral("酷狗音乐"):hash.startsWith("qishui:")?QStringLiteral("汽水音乐"):hash.startsWith("bilibili:")?QStringLiteral("哔哩哔哩"):QStringLiteral("网易云"))+QStringLiteral("在线播放");
    }
    QVector<qint64> upcoming();
    void play(const QVector<qint64> &ids, qint64 start = 0);
    void playOnline(const QVector<Track> &tracks, qint64 start);
    bool batchOnline(const QVector<Track> &tracks,const QString &action);
    bool enqueueTracks(const QVector<Track> &tracks,bool next);
    void setOnlineSource(qint64 id, const QUrl &url, const QMap<QByteArray,QByteArray> &headers={}, const QList<QUrl> &backups={});
    void refreshOnlineSource();
    void setOnlineVideoVisible(bool visible);
    bool onlineVideoVisible()const{return m_onlineVideoVisible;}
    bool wantsOnlineVideo()const{return m_onlineVideoVisible&&m_library->setting("onlineVideoMode",0).toInt()==0;}
    bool selectOnlinePart(int page,bool automatic=false);
    void setOnlineDashSource(qint64 id,const QJsonObject &video,const QJsonObject &audio,const QMap<QByteArray,QByteArray> &headers={});
    void setVideoSink(QVideoSink *sink);
    void updateOnlineTrack(const Track &track);
    void failOnlineSource(qint64 id, const QString &message, bool transient=false);
    void stopOnline();
    void toggle();
    void next();
    void previous();
    void playUpcoming(int index);
    void seek(qint64 position);
    void setMode(Mode mode);
    void setVolume(int volume);
    QVariantList audioOutputs() const;
    bool selectAudioOutput(const QString &id);
    void forget(const QVector<qint64> &ids);
    void restore(bool includeOnline=false);
    bool save();
    bool shutdown(bool force=false);
    enum class ExitDecision { Retry=0, ExitAnyway=1, Cancel=2 };
    Q_ENUM(ExitDecision)
    // Shared interactive decision boundary for both frontends.
    bool requestExit(ExitDecision decision=ExitDecision::Retry);
    QString exitError() const { return m_onlineSaveError.isEmpty()?m_playback->lastSaveError():m_onlineSaveError; }
    bool isShutdown() const { return m_playback->isShutdown(); }
  signals:
    void snapshotChanged(const relaxer::PlaybackSnapshot &snapshot);
    void trackChanged();
    void stateChanged();
    void positionChanged(qint64 position, qint64 duration);
    void seekableChanged(bool seekable);
    void error(const QString &message);
    void saveFailed(const QString &message);
    void saved(bool success);
    void playbackAttemptStarted(quint64 operation);
    void playbackFailure(quint64 operation, qint64 trackId, const QString &title,
                         const QString &message, int code);
    void playbackRecovered();
    void onlineSourceRequested(qint64 id);
    void onlineTrackUpdated(qint64 id);
    void audioOutputsChanged();

  private:
    Playback *m_playback;
    Library *m_library;
    bool m_decodeOnly=false;
    bool m_onlineActive = false, m_onlinePending = false, m_onlineFailed = false,m_onlineAutoPlay=true;
    int m_onlineRetryCount=0;
    qint64 m_onlineStableSince=0;
    QTimer m_onlineRetryTimer,m_onlineWatchdog;
    QElapsedTimer m_onlineHealth;
    std::unique_ptr<MediaPreload> m_preload;
    std::unique_ptr<DashStream> m_dash;
    QPointer<QVideoSink> m_videoSink;
    QMetaObject::Connection m_videoFrameConnection;
    bool m_videoFrameReady=false,m_onlineVideoVisible=true;
    QTimer m_onlineVideoTimer;
    std::unique_ptr<QMediaPlayer> m_onlineVideoPlayer;
    std::unique_ptr<QVideoSink> m_onlineVideoFrames;
    QTimer m_onlineVideoSync;
    QElapsedTimer m_onlineVideoAdjustment,m_onlineSeekClock;
    bool m_onlineVideoAligned=false,m_onlineVideoFailed=false;
    bool m_onlineSeekWaiting=false,m_onlineSeekAutoplay=false;
    qint64 m_onlineSeekTarget=0;
    quint64 m_onlineSeekSerial=0;
    void startOnlineVideo(const QUrl &source);
    void stopOnlineVideo(bool resumeAudio=false);
    void syncOnlineVideo(bool force=false);
    void finishOnlineVideoSeek();
    void failOnlineVideo(const QString &message);
    void updateOnlineVideoMode();
    void resetVideoFrame();
    void applyVideoSink();
    bool m_sharedQueueActive=false;
    QSet<qint64> m_queueFailures;
    Queue m_onlineQueue;
    QHash<qint64, Track> m_onlineTracks;
    std::unique_ptr<QMediaPlayer> m_onlinePlayer;
    std::unique_ptr<QAudioOutput> m_onlineAudio;
    std::unique_ptr<QMediaDevices> m_audioDevices;
    AudioFader m_onlineFader;
    qint64 m_onlineResumePosition=-1;
    QJsonObject m_savedOnline,m_savedOnlineProgress;
    QVector<qint64> m_savedOnlineIds;
    quint64 m_onlineMetadataRevision=1,m_savedOnlineRevision=0;
    bool m_savedOnlineEnabled=false;
    void pruneOnlineTracks();
    QString m_onlineSaveError;
    QMediaTimeRange bufferedTimeRanges() const;
    QMediaTimeRange m_onlineDecoded;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    std::unique_ptr<QAudioBufferOutput> m_onlineBufferOutput;
#endif
    void resetOnlineBuffer();
    void cancelOnlineStreams();
    void clearOnlineSource();
    void ensureOnlinePlayer();
    void applyOnlineResume();
    bool saveOnlineState();
    void requestOnlineCurrent(bool resume=false,bool retry=false,bool autoplay=true);
    static bool isOnline(const Track &track){return track.hash.startsWith("netease:")||track.hash.startsWith("qqmusic:")||track.hash.startsWith("kugou:")||track.hash.startsWith("qishui:")||track.hash.startsWith("bilibili:");}
    void activateQueuedCurrent();
    void advanceShared(bool manual);
    void skipUnavailableCurrent(const QString &message);
    void stopUnavailableCurrent(const QString &message);
    void sampleOnline(bool finishing=false,bool flush=false);
    void beginOnlineSession();
    void finishOnlineSession();
    Session m_onlineSession;
    Track m_onlineHistoryTrack;
    qint64 m_onlineHistoryHeard=0;
    bool m_onlineSessionMatched=false;
    int m_onlineThreshold=80;
    QElapsedTimer m_onlineClock;
    qint64 m_onlineLastWall=0,m_onlineLastPosition=0;
    std::shared_ptr<const OnlineTrackIndex> m_matchIndex;
    bool m_matchCandidatesReady=false;
    QHash<QString,qint64> m_onlineMatches;
    quint64 m_snapshotSerial=0;
    quint64 m_operation = 0;
    qint64 m_requestedTrack = 0, m_failedTrack = 0;
    QSet<QString> m_reportedFailures;
    void beginAttempt(qint64 requested = 0);
    QTimer m_saveTimer;
    enum class Notice { Track, State, Position, Seekable, Error, Snapshot };
    bool m_shuttingDown = false;
    void notify(Notice notice, qint64 position = 0, qint64 duration = 0, const QString &error = {});
    void publish();
};
} // namespace relaxer
Q_DECLARE_METATYPE(relaxer::PlaybackSnapshot)
