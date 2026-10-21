#pragma once
#include "lyrics-search-coordinator.h"
#include "playback-facade.h"
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
    Q_PROPERTY(QString targetTrackKey READ targetTrackKey NOTIFY changed)
    Q_PROPERTY(QString targetTitle READ targetTitle NOTIFY changed)
    Q_PROPERTY(QString targetArtist READ targetArtist NOTIFY changed)
    Q_PROPERTY(QString sourceText READ sourceText NOTIFY changed)
    Q_PROPERTY(QVariantList targetTracks READ targetTracks NOTIFY changed)
public:
    enum Role { CandidateKey=Qt::UserRole+1,Title,Artist,Album,Source,Timed };
    LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent=nullptr);
    int rowCount(const QModelIndex &parent={})const override;
    QVariant data(const QModelIndex &index,int role)const override;
    QHash<int,QByteArray> roleNames()const override;
    bool busy()const{return m_service->summary().running;}
    QString status()const{return m_status;}QString selectedKey()const{return m_selectedKey;}
    QString previewText()const;
    QString targetTrackKey()const{return QString::number(targetId());}
    QString targetTitle()const;QString targetArtist()const;QString sourceText()const;
    QVariantList targetTracks()const;
    Q_INVOKABLE bool setTargetTrack(const QString &key);
    Q_INVOKABLE bool setTargetTracks(const QStringList &keys,bool missingOnly=false,bool showCompleted=false);
    Q_INVOKABLE bool search(bool confirmed=false);
    Q_INVOKABLE bool searchTargets(bool confirmed=false);
    Q_INVOKABLE bool searchQuery(const QString &title,const QString &artist,const QString &fragment,bool confirmed=false,bool requireTimed=false);
    Q_INVOKABLE bool searchDetailed(const QString &title,const QString &artist,const QString &album,const QString &fragment,bool confirmed=false,bool requireTimed=false,bool replace=false);
    Q_INVOKABLE bool saveText(const QString &text,bool confirmed=false);
    Q_INVOKABLE bool importLocalLyrics(const QUrl &file,bool confirmed=false);
    Q_INVOKABLE bool markInstrumental(bool confirmed=false);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool select(int row);
    Q_INVOKABLE bool saveSelected(const QString &exactCandidateKey,bool confirmed=false);
signals:void changed();
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
    qint64 targetId()const{return m_target?m_target:m_facade->current().id;}
    bool saveBytes(const QByteArray &bytes,bool confirmed);
    void resetForTrack();
    void rememberReview(qint64 id,bool pending);
    void restoreReviews(const QVector<qint64> &ids);
};
}
