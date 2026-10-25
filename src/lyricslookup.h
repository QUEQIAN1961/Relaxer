#pragma once
#include "ai/ai-runtime.h"
#include "translation-coordinator.h"
#include "library.h"
#include "lyrics-search-coordinator.h"
#include "lyrics-replacement-service.h"
#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProgressBar>
#include <QPointer>
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
    bool importLyricsFile(const QString &path,const LyricsSaveSnapshot &expected={});
protected:
    void reject() override;
    void closeEvent(QCloseEvent *event) override;
    void done(int result) override;
private:
    Library *m_library;QVector<qint64> m_ids;QPointer<ai::AiRuntime> m_aiRuntime;
    LyricsSearchCoordinator *m_search=nullptr;
    LyricsSearchOptions searchOptions() const;
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
    QLabel *m_classificationSummary=nullptr;
    QPushButton *m_noTranslation=nullptr,*m_needsTranslation=nullptr,*m_findLyrics=nullptr;
    QWidget *m_lyricsSourcePanel=nullptr;
    void changeTranslationJudgment(TranslationJudgment requested);
    void updateTranslationWorkflow();
    bool aiTranslationAllowed(QString *reason=nullptr,bool duringConsent=false);
    QLabel *m_emptyHint=nullptr;
    QPushButton *m_refreshTranslationFilter=nullptr;
    friend class LyricsLookupDiagnostics;
    struct FilterMetrics {
        quint64 fullPasses=0,rowVisits=0,assessments=0,fileReads=0,bytesRead=0;
        quint64 digestComputations=0,digestBytes=0,auditPasses=0,auditFiles=0;
    };
    FilterMetrics m_filterMetrics;
    struct AssessmentCache {
        QString path,hash,title,artist;
        qint64 size=0,modified=0;
        bool instrumental=false,fileExists=false,readable=false,tooLarge=false,sourceValid=false;
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
    bool filterRow(int row,const LyricsFilterState *state=nullptr);
    void updateFilterRow(int row,const LyricsFilterState *state=nullptr);
    void finishFilterUpdate();
    ai::lyrics::TranslationAssessment translationAssessment(const Track &track,const bool *instrumentalState=nullptr);
    void requestTranslationCacheAudit();
    void startTranslationCacheAudit();
    void applyTranslationCacheAudit(const QVector<TranslationAuditResult> &results,const FilterMetrics &work);
    QTimer *m_translationAuditDebounce=nullptr;
    std::shared_ptr<std::atomic_bool> m_translationAuditCancel=std::make_shared<std::atomic_bool>(false);
    bool m_translationAuditInFlight=false,m_translationAuditPending=false;
    QHash<qint64,LyricsSaveSnapshot> m_lyricsSnapshots;
    QString m_storeError;
    bool m_snapshotConflict=false;
    bool m_replacementSubmitting=false;
    QString m_lastReplacementIdentity;
    QHash<qint64,QByteArray> m_manualDrafts;
    struct ReplacementContent { QByteArray bytes;bool preservesTranslation=false; };
    ReplacementContent replacementContent(const Track &track,const QJsonObject &record) const;
    void ensureLyricsSnapshot(qint64 id);
    void updateApplyButton();
    QSpinBox *m_offset;QComboBox *m_sourceFilter;QVector<int> m_visibleRecords;
    bool m_multi=false,m_moreWasOpen=false,m_pinExplicit=false;
    QPushButton *m_multiButton=nullptr,*m_allButton=nullptr,*m_moreButton=nullptr,*m_pasteButton=nullptr,*m_importButton=nullptr,*m_saveOffset=nullptr;
    QWidget *m_morePanel=nullptr;
    QSet<qint64> m_missingAtOpen,m_pinnedIds;
    TranslationCoordinator *m_translation=nullptr;
    quint64 m_translationTaskId=0; // Frontend identity only; service owns all state.
    bool m_aiConsentPending=false;
    bool m_translationCloseConfirmOpen=false;
    bool m_translationSwitchConfirmOpen=false;
    qint64 trackIdAt(int row) const;
    void restoreTranslationSelection();
    void trackSelectionChanged();
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
    void start();void stop();
    void manualSearch();void selectTrack();void rebuildCandidates();bool applySelected();
    void setRowStatus(int row,const QString &text);void updateFilter();void controls();
    bool store(const Track &track,const QByteArray &data,bool force=false,bool preserveText=false);
    bool translationPending() const;
    bool confirmStopTranslations();
    void translateSelected();
    void remember(qint64 id);
    bool saveManual(const QByteArray &data,const QString &status,const QString &candidateIdentity={});
    QString selectedReplacementIdentity() const;
    bool confirmReplacement(const LyricsReplacementPlan &plan);
    void pasteLyrics();void importLyrics();void markInstrumental();
    void selectNextIncomplete(int after);
    QVector<int> batchRows() const;void setMultiSelect(bool on);void saveOffsets();
};
}
