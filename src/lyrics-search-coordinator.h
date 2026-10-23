#pragma once
#include "library.h"
#include "lyric-remote.h"
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QThreadPool>
#include <atomic>
#include <memory>

namespace relaxer {
struct LyricsSearchSource {
    QString label;
    QUrl endpoint;
    int kind = 0;
    bool backup = false;
};
struct LyricsQueryAttempt {
    QString title, artist, album, keywords;
    int source = 0;
    bool exact = false, keywordsSearch = false, cover = false, detailRequest = false;
    lyric_remote::Detail detail;
};
struct LyricsSearchOptions {
    int budgetMs = 30000, requestTimeoutMs = 8000, retryDelayMs = 500, scoreDelayMs = 0;
};
struct LyricsSearchRequest {
    Track original, match;
    QString fragment;
    bool manual = false, replace = false, requireTimed = false;
    // Optional frontend snapshot preserves an already observed external-change conflict.
    LyricsSaveSnapshot saveSnapshot;
};
struct LyricsQueryPlan {
    LyricsSearchRequest request;
    LyricsSearchOptions options;
    QVector<LyricsQueryAttempt> attempts;
};
enum class LyricsSearchState {
    Queued,
    Searching,
    CandidatesFound,
    AwaitingChoice,
    Saved,
    NoResult,
    Failed,
    Invalidated,
    Cancelled
};
struct LyricsCandidateSnapshot {
    qint64 trackId = 0;
    QString trackHash;
    quint64 revision = 0;
    QJsonArray records;
};
struct LyricsSearchEvent {
    quint64 taskId = 0, generation = 0, candidateRevision = 0;
    qint64 trackId = 0;
    LyricsSearchState state = LyricsSearchState::Queued;
    QString phase, status, source;
    int attempt = -1, pass = 0;
    QUrl url;
};
struct LyricsSearchSummary {
    quint64 generation = 0;
    bool running = false;
    int requested = 0, finished = 0, candidateTracks = 0, matchedTracks = 0, automaticAccepted = 0, saved = 0,
        cancelled = 0, failed = 0;
    int requests = 0, activeTracks = 0, activeRequests = 0, trackLimit = 10, requestLimit = 3;
    QString status;
};
class LyricsSearchCoordinator final : public QObject {
    Q_OBJECT
  public:
    explicit LyricsSearchCoordinator(Library *library, QObject *parent = nullptr,
                                     QUrl endpoint = QUrl("https://lrclib.net/api"), QUrl backupEndpoint = {},
                                     QNetworkAccessManager *transport = nullptr,
                                     QVector<LyricsSearchSource> sources = {});
    ~LyricsSearchCoordinator() override;
    static QVector<LyricsSearchSource> defaultSources(QUrl endpoint, QUrl backup = {});
    static LyricsQueryPlan queryPlan(const LyricsSearchRequest &request,
                                     const QVector<LyricsSearchSource> &sources,
                                     const LyricsSearchOptions &options = {});
    quint64 start(const QVector<LyricsSearchRequest> &requests, const LyricsSearchOptions &options = {});
    QVector<LyricsSearchEvent> stop();
    void shutdown();
    void cancelTrack(qint64 id);
    void loadCached(const QVector<qint64> &ids);
    void clearCandidates(qint64 id);
    void remember(qint64 id);
    void noteManualSave(int count = 1);
    LyricsCandidateSnapshot candidates(qint64 id) const { return m_candidates.value(id); }
    LyricsSearchSummary summary() const { return m_summary; }
    QVector<LyricsSearchSource> sources() const { return m_sources; }
    static QString candidateIdentity(const QJsonObject &record);
    static QVector<QJsonObject> rankCandidates(const Track &query, const QVector<QJsonObject> &records);
    static double score(const Track &track, const QJsonObject &record);
    static void resetMatchScoreCount();
    static qint64 matchScoreCount();
    static void resetRankRuns();
    static qint64 rankRuns();
    static QThreadPool *computePool();
    static std::atomic_bool &computeCancel();
  signals:
    void eventOccurred(const relaxer::LyricsSearchEvent &event);
    void candidatesChanged(const relaxer::LyricsCandidateSnapshot &snapshot, quint64 generation);
    void summaryChanged(const relaxer::LyricsSearchSummary &summary);
    void taskFinished(const relaxer::LyricsSearchEvent &event);
    void finished(const relaxer::LyricsSearchSummary &summary);

  private:
    struct RankItem {
        QJsonObject record;
        double score = 0;
        bool confidentOriginal = false, credible = false, confidentCover = false;
        QString rejection;
    };
    struct RankResult {
        qint64 id = 0;
        QString hash, fpO, fpM;
        int generation = 0;
        quint64 epoch = 0;
        bool cached = false, timedRequired = false, dropped = false;
        QJsonArray payload;
        QVector<RankItem> ranked, all;
    };
    struct ScoreSnap {
        QJsonArray payload;
        Track original, match;
        qint64 id = 0;
        QString hash, fpO, fpM;
        int generation = 0, delayMs = 0;
        quint64 epoch = 0;
        bool cached = false, timedRequired = false;
    };
    static RankResult computeRank(const ScoreSnap &snap);
    using Source = LyricsSearchSource;
    using Attempt = LyricsQueryAttempt;
    struct Job {
        LyricsSearchRequest input;
        LyricsSaveSnapshot saveSnapshot;
        bool terminal = false, onlineFinished = false;
        Track original, match;
        quint64 taskId = 0;
        int generation = 0, next = 0, active = 0, searchPass = 0;
        bool manual = false, completeResponses = true, onlineStarted = false;
        QVector<Attempt> attempts;
        QList<QPair<int,int>> deferredRequests;
        QJsonArray collected;
        QSet<QString> keys;
        QStringList failures;
        QSet<QUrl> detailUrls;
        QSet<QNetworkReply *> replies;
        QSet<quint64> inflight;
        QPointer<QProcess> probe;
        QElapsedTimer clock;
        quint64 collectionEpoch = 0, cacheEpoch = 0;
        bool scoreInFlight = false, wantAccept = false, acceptCached = false;
        bool readyValid = false, readyCached = false, readyTimed = false;
        quint64 readyEpoch = 0;
        QString readyFpO, readyFpM;
        QJsonArray cachedPayload;
        QVector<RankItem> readyRanked, readyAll;
    };
    using JobPtr = std::shared_ptr<Job>;
    QPointer<Library> m_library;
    QPointer<QNetworkAccessManager> m_network;
    QVector<Source> m_sources;
    QHash<qint64, JobPtr> m_jobs;
    QVector<JobPtr> m_recoveryQueue;
    QVector<LyricsSearchRequest> m_queue;
    QHash<qint64, LyricsCandidateSnapshot> m_candidates;
    QHash<QString, QVector<qint64>> m_reuse;
    QHash<QString, qint64> m_cooldowns;
    QHash<QString, int> m_hostRequests;
    QSet<QString> m_rateLimitedHosts;
    LyricsSearchSummary m_summary;
    LyricsSearchOptions m_options;
    QSet<qint64> m_found, m_matched;
    int m_generation = 0, m_next = 0, m_saved = 0, m_trackLimit = 10, m_requestLimit = 3, m_fastReplies = 0;
    quint64 m_candidateRevision = 0;
    bool m_running = false, m_proxyReady = true, m_stopping = false;
    std::shared_ptr<std::atomic_bool> m_proxyState;
    QString m_storeError;
    bool ownerThread() const;
    void pumpTracks();
    void lookup(const JobPtr &job);
    void beginOnline(const JobPtr &job);
    void pumpQueries(const JobPtr &job);
    void request(const JobPtr &job, int stage, int retry = 0);
    void acceptRecords(const JobPtr &job, const QJsonArray &records, bool cached = false);
    void scheduleRanking(const JobPtr &job);
    void applyRanking(const JobPtr &job, const RankResult &result);
    void applyAccept(const JobPtr &job);
    bool rankingCurrent(const JobPtr &job) const;
    bool cacheRankingCurrent(const JobPtr &job) const;
    bool earlyStopReady(const JobPtr &job) const;
    void appendCollected(const JobPtr &job, const QJsonObject &record);
    static QString trackContext(const Track &track);
    void finishOnline(const JobPtr &job);
    void finishJob(const JobPtr &job, const QString &status,
                   LyricsSearchState state = LyricsSearchState::AwaitingChoice);
    bool live(const JobPtr &job) const;
    bool current(const JobPtr &job) const;
    void detachRequests(const JobPtr &job);
    void releaseRequestSlot(QNetworkReply *reply);
    void clearTransientCooldowns();
    bool store(const Track &track, const QByteArray &bytes, bool force = false, bool preserveText = false);
    bool productionHost() const;
    QString reuseKey(const Track &track) const;
    void setJobStatus(const JobPtr &job, const QString &status, QString phase = "progress", int attempt = -1,
                      QUrl url = {}, QString source = {});
    void publishEvent(LyricsSearchEvent event, bool terminal = false);
    void publishSummary(bool terminal = false);
    void setCandidates(const JobPtr &job, const QJsonArray &records);
};
} // namespace relaxer
Q_DECLARE_METATYPE(relaxer::LyricsSearchEvent)
Q_DECLARE_METATYPE(relaxer::LyricsCandidateSnapshot)
Q_DECLARE_METATYPE(relaxer::LyricsSearchSummary)
