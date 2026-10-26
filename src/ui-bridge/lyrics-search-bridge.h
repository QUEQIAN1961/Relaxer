#pragma once
#include "lyrics-search-coordinator.h"
#include "playback-facade.h"
#include "lyrics-replacement-service.h"
#include "lyrics-edit-service.h"
#include "lyrics-translation-classification.h"
#include <QAbstractListModel>
#include <QVariantList>
#include <QSet>
namespace relaxer {
class LyricsSearchBridge final:public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString selectedKey READ selectedKey NOTIFY changed)
    Q_PROPERTY(QString previewText READ previewText NOTIFY changed)
    Q_PROPERTY(QString draftText READ draftText NOTIFY changed)
    Q_PROPERTY(QString targetTrackKey READ targetTrackKey NOTIFY changed)
    Q_PROPERTY(QString targetTitle READ targetTitle NOTIFY changed)
    Q_PROPERTY(QString targetArtist READ targetArtist NOTIFY changed)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY changed)
    Q_PROPERTY(QVariantList targetTracks READ targetTracks NOTIFY changed)
    Q_PROPERTY(QVariantList availableSources READ availableSources NOTIFY sourcesChanged)
    Q_PROPERTY(QStringList enabledSources READ enabledSources NOTIFY sourcesChanged)
    Q_PROPERTY(QString candidateSource READ candidateSource WRITE setCandidateSource NOTIFY changed)
    Q_PROPERTY(QString recommendedKey READ recommendedKey NOTIFY changed)
    Q_PROPERTY(QString searchContext READ searchContext NOTIFY changed)
    Q_PROPERTY(QVariantMap fileOffset READ fileOffset NOTIFY changed)
    Q_PROPERTY(bool batchPrepared READ batchPrepared NOTIFY changed)
    Q_PROPERTY(bool batchRunning READ batchRunning NOTIFY changed)
    Q_PROPERTY(QVariantMap batchOverview READ batchOverview NOTIFY changed)
    Q_PROPERTY(QVariantMap batchResult READ batchResult NOTIFY changed)
public:
    enum Role { CandidateKey=Qt::UserRole+1,Title,Artist,Album,Source,Timed,CandidateVisible,MatchDetails };
    LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent=nullptr);
    int rowCount(const QModelIndex &parent={})const override;
    QVariant data(const QModelIndex &index,int role)const override;
    QHash<int,QByteArray> roleNames()const override;
    bool busy()const{return m_service->summary().running;}
    QString status()const{return m_status;}QString selectedKey()const{return m_selectedKey;}
    QString previewText()const;
    QString draftText()const;
    QString targetTrackKey()const{return QString::number(targetId());}
    QString targetTitle()const;QString targetArtist()const;QString sourceText()const;
    QVariantList targetTracks()const;
    QVariantList availableSources() const;
    QStringList enabledSources() const { return m_enabledSources; }
    QString candidateSource() const { return m_candidateSource; }
    void setCandidateSource(const QString &source);
    QString recommendedKey() const;
    QString searchContext() const { return m_searchContext; }
    Q_INVOKABLE QVariantMap defaultQuery() const;
    Q_INVOKABLE bool setSourceEnabled(const QString &id,bool enabled);
    Q_INVOKABLE void restoreSearchDefaults();
    Q_INVOKABLE void parametersEdited();
    Q_INVOKABLE void setSearchOptions(bool requireTimed,bool replace);
    Q_INVOKABLE bool setTargetTrack(const QString &key);
    Q_INVOKABLE bool setTargetTracks(const QStringList &keys,bool missingOnly=false,bool showCompleted=false);
    Q_INVOKABLE bool search(bool confirmed=false);
    Q_INVOKABLE bool searchTargets(bool confirmed=false);
    Q_INVOKABLE bool searchTracks(const QStringList &keys,bool confirmed=false);
    Q_INVOKABLE bool searchQuery(const QString &title,const QString &artist,const QString &fragment,bool confirmed=false,bool requireTimed=false);
    Q_INVOKABLE bool searchDetailed(const QString &title,const QString &artist,const QString &album,const QString &fragment,bool confirmed=false,bool requireTimed=false,bool replace=false,double durationMs=-1);
    Q_INVOKABLE bool requestLocalImport(const QString &key);
    Q_INVOKABLE QString beginLocalEdit();
    Q_INVOKABLE bool saveText(const QString &text,bool confirmed=false,const QString &editToken={});
    Q_INVOKABLE bool importLocalLyrics(const QUrl &file,bool confirmed=false,const QString &editToken={});
    Q_INVOKABLE bool markInstrumental(bool confirmed=false);
    QVariantMap fileOffset() const { return m_offsetView; }
    Q_INVOKABLE bool beginFileOffset();
    Q_INVOKABLE bool previewFileOffset(double milliseconds);
    Q_INVOKABLE bool saveFileOffset(bool discardTranslation=false);
    Q_INVOKABLE void cancelFileOffset();
    bool batchPrepared() const { return m_batchPrepared; }
    bool batchRunning() const { return m_batchRunning; }
    QVariantMap batchOverview() const { return m_batch.overview(); }
    QVariantMap batchResult() const { return m_batch.result(); }
    Q_INVOKABLE bool prepareInstrumentalBatch(const QStringList &keys);
    Q_INVOKABLE bool startInstrumentalBatch(bool replaceRisks=false);
    Q_INVOKABLE void cancelInstrumentalBatch();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool select(int row);
    Q_INVOKABLE bool selectKey(const QString &key);
    Q_INVOKABLE bool selectRecommended(bool requireTimed = false);
    Q_INVOKABLE bool saveSelected(const QString &exactCandidateKey,bool confirmed=false);
    Q_INVOKABLE bool resolveReplacement(const QString &token,int choice);
    Q_INVOKABLE void cancelReplacement(const QString &token);
    void setTranslationCoordinator(TranslationCoordinator *service){m_translation=service;}
signals:
    void operationFailed(const QString &target, const QString &message);
    void changed();
    void sourcesChanged();
    void localImportRequested(const QString &key);
    void replacementRequested(const QString &token,const QString &reason);
    void saved(const QString &trackKey);
    void editCompleted(const QString &category,const QString &operation,const QString &message,const QString &details);
    void searchFeedback(const QString &category,const QString &operation,const QString &target,const QString &message,const QString &details);
private:
    Library *m_library;PlaybackFacade *m_facade;LyricsSearchCoordinator *m_service;
    LyricsCandidateSnapshot m_snapshot;LyricsSaveSnapshot m_guard;
    QJsonObject m_selected;QString m_selectedKey,m_status;
    qint64 m_target=0;
    QVector<qint64> m_targets;
    QHash<qint64,Track> m_targetRows;
    QHash<qint64,QString> m_trackStatus;
    QHash<qint64,LyricsSearchState> m_trackState;
    QSet<qint64> m_cachedChoices;
    QSet<qint64> m_pendingReviews;
    bool m_missingOnly=false;
    bool m_recommendTimedOnly=false;
    bool m_explicitSelection=false;
    QStringList m_enabledSources;
    QString m_candidateSource,m_searchContext;
    QHash<qint64,QString> m_searchContexts;
    QSet<qint64> m_searchIds,m_sourceFailures;
    bool m_searchBatch=false,m_requireTimed=false,m_replaceSearch=false;
    bool candidateVisible(const QJsonObject &record) const;
    void refreshCandidateFilter();
    bool searchInvalid(const QString &message);
    struct CachedClassification { QByteArray signature; TranslationClassification value; };
    mutable QHash<qint64,CachedClassification> m_translationCache;
    TranslationClassification translationClassification(const Track &track)const;
    bool translationMissing(const Track &track)const;
    bool startTargets(const QVector<qint64> &ids,bool confirmed,bool missingOnly);
    qint64 targetId()const{return m_target?m_target:m_facade->current().id;}
    bool saveBytes(const QByteArray &bytes,bool confirmed,const QString &editToken={});
    QString m_editToken;
    LyricsSaveSnapshot m_editGuard;
    QHash<qint64,QByteArray> m_manualDrafts;
    bool m_showDraft=false;
    TranslationCoordinator *m_translation=nullptr;
    LyricsReplacementPlan m_plan;
    QString m_planToken,m_planSelection,m_lastSavedIdentity;
    bool m_submitting=false;
    bool prepareSave(const QByteArray &bytes,const QString &identity,const LyricsSaveSnapshot &snapshot,bool selected);
    bool commitPlan(LyricsReplacementChoice choice);
    void invalidatePlan();
    void resetForTrack();
    void rememberReview(qint64 id,bool pending);
    void restoreReviews(const QVector<qint64> &ids);
    LyricsOffsetPlan m_offsetPlan;
    LyricsSaveSnapshot m_offsetSnapshot;
    QVariantMap m_offsetView;
    LyricsInstrumentalBatch m_batch;
    QString m_batchOperation;
    bool m_batchPrepared=false,m_batchRunning=false,m_batchReplace=false,m_batchStep=false,m_batchCancelRequested=false;
    quint64 m_batchEpoch=0;
    void processInstrumental(quint64 epoch);
    void finishInstrumental();
};
}
