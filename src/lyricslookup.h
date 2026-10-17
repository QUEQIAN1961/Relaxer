#pragma once
#include "ai/ai-runtime.h"
#include "translation-coordinator.h"
#include "library.h"
#include "lyric-remote.h"
#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPointer>
#include <QProcess>
#include <QElapsedTimer>
#include <QSet>
#include <memory>
#include <atomic>
#include <functional>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
class QLabel;class QListWidget;class QPlainTextEdit;class QFrame;class QPushButton;class QLineEdit;class QCheckBox;class QSpinBox;class QComboBox;class QThreadPool;class QCloseEvent;class QTimer;
namespace relaxer {
struct LyricHttpResult {
    quint64 token = 0;
    QByteArray body, retryAfter, remotePhase;
    int status = 0, error = 0;
    bool timedOut = false, tooLarge = false, remoteParsed = false;
    qint64 remoteDuration = 0;
    QJsonParseError parseError{};
    QJsonDocument doc;
    lyric_remote::Follow follow;
};
class LyricsLookup : public QDialog {
    Q_OBJECT
public:
    LyricsLookup(Library *library,QVector<qint64> ids,QWidget *parent=nullptr,QUrl endpoint=QUrl("https://lrclib.net/api"),QUrl backupEndpoint={},bool pinExplicit=false,ai::AiRuntime *aiRuntime=nullptr);
    ~LyricsLookup() override;
    static QString normalized(const QString &text);
    static QString cleanTitle(const QString &text);
    static Track searchTrack(const Track &track);
    static bool confident(const Track &track,const QJsonObject &record);
    static double matchScore(const Track &track,const QJsonObject &record);
    static QByteArray withOffset(const QByteArray &data,int offsetMs);
    static QString matchReason(const Track &track,const QJsonObject &record);
    static bool credibleLyrics(const QByteArray &data,const Track &track,QString *reason=nullptr);
    static void resetMatchScoreCount();
    static qint64 matchScoreCount();
    static void resetLyricRankRuns();
    static qint64 lyricRankRuns();
    static int lyricComputeThreadCount();
    static QVector<QJsonObject> rankCandidates(const Track &query,const QVector<QJsonObject> &usable);
protected:
    void reject() override;
    void closeEvent(QCloseEvent *event) override;
private:
    struct RankItem {
        QJsonObject record;
        double score=0;
        bool confidentOriginal=false,credible=false,confidentCover=false;
        QString rejection;
    };
    struct RankResult {
        qint64 id=0;QString hash,fpO,fpM;int generation=0;quint64 epoch=0;
        bool cached=false,timedRequired=false,dropped=false;
        QJsonArray payload;QVector<RankItem> ranked,all;
    };
    struct ScoreSnap {
        QJsonArray payload;Track original,match;qint64 id=0;QString hash,fpO,fpM;
        int generation=0,delayMs=0;quint64 epoch=0;bool cached=false,timedRequired=false;
    };
    static RankResult computeRank(const ScoreSnap &snap);
    struct Source {QString label;QUrl endpoint;int kind=0;bool backup=false;};
    struct Attempt {QString title,artist,album,keywords;int source=0;bool exact=false,keywordsSearch=false,cover=false;bool detailRequest=false;lyric_remote::Detail detail;};
    struct Job {
        Track original,match;int row=-1,generation=0,next=0,active=0,searchPass=0;bool manual=false,completeResponses=true,onlineStarted=false;
        QVector<Attempt> attempts;QJsonArray collected;QSet<QString> keys;QStringList failures;
        QSet<QUrl> detailUrls;
        QSet<QNetworkReply *> replies;QSet<quint64> inflight;QPointer<QProcess> probe;QElapsedTimer clock;
        quint64 collectionEpoch=0,cacheEpoch=0;
        bool scoreInFlight=false,wantAccept=false,acceptCached=false;
        bool readyValid=false,readyCached=false,readyTimed=false;
        quint64 readyEpoch=0;QString readyFpO,readyFpM;QJsonArray cachedPayload;
        QVector<RankItem> readyRanked,readyAll;
    };
    using JobPtr=std::shared_ptr<Job>;
    Library *m_library;QVector<qint64> m_ids;QVector<Source> m_sources;QPointer<ai::AiRuntime> m_aiRuntime;
    QNetworkAccessManager m_network;
    QHash<qint64,JobPtr> m_jobs;
    QVector<JobPtr> m_recoveryQueue;
    QHash<qint64,QJsonArray> m_candidates;QHash<qint64,QString> m_candidateHashes;
    QHash<QString,QVector<qint64>> m_reuse;QHash<QString,qint64> m_cooldowns;
    QHash<QString,int> m_hostRequests;QSet<QString> m_rateLimitedHosts;
    void releaseRequestSlot(QNetworkReply *reply);
    int hostCap() const;
    int cpuCount() const;
    void resetParallelism();
    void deliver(const JobPtr &job,const QNetworkRequest &req,int timeoutMs,const QString &host,const QByteArray &phase,qint64 duration,bool countHost,const std::function<void(LyricHttpResult &)> &prepare,const std::function<void(const LyricHttpResult &)> &done);
    QListWidget *m_tracks=nullptr,*m_versions=nullptr;
    QPlainTextEdit *m_preview=nullptr,*m_translationPreview=nullptr;
    QFrame *m_candidatePanel=nullptr,*m_previewPanel=nullptr;
    QWidget *m_translationProgressPanel=nullptr;
    QProgressBar *m_translationProgress=nullptr;
    QLabel *m_translationProgressLabel=nullptr;
    bool m_applyingStyle=false;
    QLabel *m_status,*m_candidateTitle,*m_instrumentHint;
    QPushButton *m_start,*m_stop,*m_apply,*m_incomplete=nullptr,*m_manualSearch,*m_mark,*m_continue,*m_translate=nullptr,*m_aiSaveTranslation=nullptr,*m_aiCancelTranslation=nullptr;
    QLineEdit *m_searchTitle,*m_searchArtist,*m_fragment;QCheckBox *m_replace,*m_syncedOnly,*m_missingOnly=nullptr,*m_missingTranslationOnly=nullptr;
    QLabel *m_emptyHint=nullptr;
    QPushButton *m_refreshTranslationFilter=nullptr;
    friend class LyricsLookupDiagnostics;
    struct FilterMetrics {
        quint64 fullPasses=0,rowVisits=0,assessments=0,fileReads=0,bytesRead=0,jsonKeys=0;
        quint64 digestComputations=0,digestBytes=0,auditPasses=0,auditFiles=0;
    };
    FilterMetrics m_filterMetrics;
    struct AssessmentCache {
        QString path,hash,title,artist;
        qint64 size=0,modified=0;
        bool instrumental=false,fileExists=false,readable=false,tooLarge=false;
        quint64 revision=0;
        QByteArray contentDigest;
        ai::lyrics::TranslationAssessment value;
    };
    struct TranslationAuditSnapshot {
        qint64 id=0;
        int row=-1;
        QString path;
        Track track;
        bool instrumental=false;
        AssessmentCache previous;
    };
    struct TranslationAuditResult {
        TranslationAuditSnapshot snapshot;
        AssessmentCache current;
        bool readAttempted=false,digestComputed=false;
        qint64 bytesRead=0,digestBytes=0;
    };
    QHash<qint64,AssessmentCache> m_translationAssessments;
    quint64 m_translationAssessmentRevision=0;
    int m_filterVisibleCount=0;
    bool m_filterReady=false,m_filterUpdating=false;
    bool filterRow(int row);
    void updateFilterRow(int row);
    void finishFilterUpdate();
    ai::lyrics::TranslationAssessment translationAssessment(const Track &track);
    void requestTranslationCacheAudit();
    void startTranslationCacheAudit();
    void applyTranslationCacheAudit(const QVector<TranslationAuditResult> &results,const FilterMetrics &work);
    QTimer *m_translationAuditDebounce=nullptr;
    std::shared_ptr<std::atomic_bool> m_translationAuditCancel=std::make_shared<std::atomic_bool>(false);
    bool m_translationAuditInFlight=false,m_translationAuditPending=false;
    QHash<qint64,LyricsSaveSnapshot> m_lyricsSnapshots;
    QString m_storeError;
    bool m_snapshotConflict=false;
    struct ReplacementContent { QByteArray bytes;bool preservesTranslation=false; };
    ReplacementContent replacementContent(const Track &track,const QJsonObject &record) const;
    void ensureLyricsSnapshot(qint64 id);
    bool checkLyricsSnapshot(qint64 id);
    void updateApplyButton();
    QSpinBox *m_offset;QComboBox *m_sourceFilter;QVector<int> m_visibleRecords;
    int m_generation=0,m_nextRow=0,m_saved=0,m_trackLimit=10,m_requestLimit=3,m_fastReplies=0;
    bool m_running=false,m_proxyReady=true;
    std::shared_ptr<std::atomic_bool> m_proxyState;
    QThreadPool *m_netPool=nullptr;
    quint64 m_fetchSerial=0;
    QHash<quint64,std::shared_ptr<std::atomic<bool>>> m_fetchCancel;
    QSet<quint64> m_openFetches;
    QHash<quint64,QString> m_fetchHost;
    bool m_multi=false,m_moreWasOpen=false,m_batchJobScope=false,m_pinExplicit=false;
    QPushButton *m_multiButton=nullptr,*m_allButton=nullptr,*m_moreButton=nullptr,*m_pasteButton=nullptr,*m_importButton=nullptr,*m_saveOffset=nullptr;
    QWidget *m_morePanel=nullptr;
    QSet<int> m_requestedRows;
    QSet<qint64> m_missingAtOpen,m_pinnedIds;
    TranslationCoordinator *m_translation=nullptr;
    quint64 m_translationTaskId=0; // Frontend identity only; service owns all state.
    bool m_aiConsentPending=false;
    TranslationSummary aiSummary() const;
    quint64 aiTaskId() const;
    qint64 aiTrackId() const;
    bool hasAiPreview() const;
    void renderAiTranslation();
    void updatePreview();
    void setPreviewLyrics(const QByteArray &data);
    void setAiTranslationPreview();
    void saveAiTranslation();
    void clearAiTranslationPreview();
    void cancelAiTranslation(const QString &reason={});
    void cancelAiTranslationForTrack(qint64 id);
    void resetTranslationProgress();
    void setTranslationProgress(const QString &status,const QString &details,int value=0);
    int rowForTrack(qint64 id) const;
    void applyLookupStyle();
    void changeEvent(QEvent *event) override;
    void start();void stop();void pumpTracks();void lookup(const JobPtr &job);
    void beginOnline(const JobPtr &job);void pumpQueries(const JobPtr &job);void request(const JobPtr &job,int stage,int retry=0);
    void acceptRecords(const JobPtr &job,const QJsonArray &records,bool cached=false);
    void scheduleRanking(const JobPtr &job);
    void applyRanking(const JobPtr &job,const RankResult &result);
    void applyAccept(const JobPtr &job);
    bool rankingCurrent(const JobPtr &job) const;
    bool cacheRankingCurrent(const JobPtr &job) const;
    bool earlyStopReady(const JobPtr &job) const;
    void appendCollected(const JobPtr &job,const QJsonObject &record);
    QString trackContext(const Track &track) const;
    void finishOnline(const JobPtr &job);void finishJob(const JobPtr &job,const QString &status);
    bool live(const JobPtr &job) const;void cancelJob(qint64 id);
    void clearTransientCooldowns();
    void manualSearch();void selectTrack();void rebuildCandidates();bool applySelected();
    void setRowStatus(int row,const QString &text);void updateFilter();void controls();
    bool store(const Track &track,const QByteArray &data,bool force=false,bool preserveText=false);
    bool productionHost() const;
    bool translationPending() const;
    bool confirmStopTranslations();
    void translateSelected();
    void remember(qint64 id);QString reuseKey(const Track &track) const;
    bool saveManual(const QByteArray &data,const QString &status,bool preserveText=false);void pasteLyrics();void importLyrics();void markInstrumental();
    void selectNextIncomplete(int after);
    QVector<int> batchRows() const;void setMultiSelect(bool on);void saveOffsets();
};
}
