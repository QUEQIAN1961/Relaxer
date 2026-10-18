#pragma once
#include "lyrics-search-coordinator.h"
#include "playback-facade.h"
#include <QAbstractListModel>
namespace relaxer {
class LyricsSearchBridge final:public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString selectedKey READ selectedKey NOTIFY changed)
    Q_PROPERTY(QString previewText READ previewText NOTIFY changed)
public:
    enum Role { CandidateKey=Qt::UserRole+1,Title,Artist,Album,Source,Timed };
    LyricsSearchBridge(Library *library,PlaybackFacade *facade,LyricsSearchCoordinator *service,QObject *parent=nullptr);
    int rowCount(const QModelIndex &parent={})const override;
    QVariant data(const QModelIndex &index,int role)const override;
    QHash<int,QByteArray> roleNames()const override;
    bool busy()const{return m_service->summary().running;}
    QString status()const{return m_status;}QString selectedKey()const{return m_selectedKey;}
    QString previewText()const;
    Q_INVOKABLE bool search(bool confirmed=false);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool select(int row);
    Q_INVOKABLE bool saveSelected(const QString &exactCandidateKey,bool confirmed=false);
signals:void changed();
private:
    Library *m_library;PlaybackFacade *m_facade;LyricsSearchCoordinator *m_service;
    LyricsCandidateSnapshot m_snapshot;LyricsSaveSnapshot m_guard;
    QJsonObject m_selected;QString m_selectedKey,m_status;
    void resetForTrack();
};
}
