#pragma once
#include "../ai/ai-runtime.h"
#include "lyrics-model.h"
#include "online-page-cache.h"
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QPointer>
#include <QVariantList>
#include <functional>
#include <memory>

namespace relaxer {
class OnlineArtworkCache;
class MusicCookieJar final:public QNetworkCookieJar {
public:
    using QNetworkCookieJar::QNetworkCookieJar;
    QByteArray serialize() const;
    void restore(const QByteArray &bytes);
    bool hasSession() const;
    static QByteArray protect(const QByteArray &bytes,bool decrypt);
};
class OnlineMusicBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY changed)
    Q_PROPERTY(QString nickname READ nickname NOTIFY changed)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY changed)
    Q_PROPERTY(QVariantList songs READ songs NOTIFY songsChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(QVariantMap currentPlaylist READ currentPlaylist NOTIFY changed)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY changed)
public:
    OnlineMusicBridge(Library *library,PlaybackFacade *facade,LyricsModel *lyrics,ai::AiRuntime *ai,OnlineArtworkCache *artwork,QObject *parent=nullptr);
    ~OnlineMusicBridge() override;
    bool loggedIn() const{return m_loggedIn;}
    bool busy() const{return !m_pageCache.quiet()&&(!m_requests.isEmpty()||m_aiTask!=0);}
    bool catalogLoading() const{return m_pageCache.pending();}
    QString nickname() const{return m_nickname;}
    QUrl avatarUrl() const{return m_avatarUrl;}
    QString status() const{return m_status;}
    QString qrImage() const{return m_qrImage;}
    QVariantList songs() const{return m_songs;}
    QVariantList playlists() const{return m_playlists;}
    QVariantList searchResults() const{return m_searchResults;}
    QVariantMap currentPlaylist() const{return m_currentPlaylist;}
    QString quality() const{return m_quality;}
    void setQuality(const QString &quality);
    Q_INVOKABLE void activate();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void stopLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void search(const QString &query,int category=0);
    Q_INVOKABLE void dailyRecommendations();
    Q_INVOKABLE void loadPlaylists();
    Q_INVOKABLE void recommendedPlaylists();
    Q_INVOKABLE void showPage(const QString &kind,const QString &argument,bool refresh);
    Q_INVOKABLE void downloadSong(const QString &id);
    void resolveDownload(const QVariantMap &song,const QString &quality);
    Q_INVOKABLE void openPlaylist(const QString &id);
    Q_INVOKABLE void recommend();
    Q_INVOKABLE void playSong(const QString &id);
    void playSongs(const QStringList &ids,const QString &start);
    void savePinnedPlaylist(const QString &id){m_pageCache.savePinned("playlist:"+id);}
    Q_INVOKABLE void cancelRecommendation();
    void toggleFavorite(qint64 internalId);
    static constexpr qint64 TrackBase=1000000000000LL;
signals:
    void changed();
    void songsChanged();
    void downloadReady(const QVariantMap &song,const QUrl &url,const QString &extension);
    void downloadFailed(const QVariantMap &song,const QString &error);
private:
    Library *m_library;PlaybackFacade *m_facade;LyricsModel *m_lyrics;ai::AiRuntime *m_ai;
    OnlineArtworkCache *m_artwork;
    OnlinePageCache m_pageCache;bool m_forcePage=false;QTimer m_pageCommit;
    QNetworkAccessManager m_network;MusicCookieJar *m_cookies;
    QSet<QNetworkReply *> m_requests;
    QPointer<QNetworkReply> m_loginPoll;
    QTimer m_poll;
    QUrl m_avatarUrl;
    QString m_nickname,m_status=QStringLiteral("连接网易云账号，发现更多音乐"),m_qrImage,m_qrKey,m_quality="hires";
    QString m_userId,m_deviceId;bool m_loggedIn=false,m_activated=false;
    QVariantList m_songs,m_playlists,m_searchResults;QHash<QString,QVariantMap> m_collections;QHash<qint64,QVariantMap> m_catalog;
    QVariantMap m_currentPlaylist;
    quint64 m_loginRevision=0,m_listRevision=0,m_aiRevision=0,m_aiTask=0,m_playRevision=0;
    QString onlinePath(const QString &file) const;
    QNetworkReply *post(const QString &path,const QJsonObject &data,std::function<void(const QJsonObject &)> done);
    void traceLogin(const QString &message) const;
    void setStatus(const QString &status);
    void refreshAccount();void pollLogin();void saveSession();void restoreSession();
    void setSongs(const QJsonArray &songs);
    struct PlaylistLoad;
    void fetchPlaylistBatch(const std::shared_ptr<PlaylistLoad> &load);
    void finishPlaylist(const std::shared_ptr<PlaylistLoad> &load,bool failed=false);
    QVariantMap song(const QJsonObject &object) const;
    void requestSource(qint64 internalId);
    void loadExtras(qint64 internalId);
    void rankWithAi();
    bool beginPage(const QString &key);
    void openCollection(const QString &kind,const QString &id);
};
}
