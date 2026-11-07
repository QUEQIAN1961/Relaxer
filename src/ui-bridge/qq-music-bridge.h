#pragma once
#include "online-music-bridge.h"

namespace relaxer {
// QQ and WeChat authorize the QQ Music account, then use the returned music
// credential for catalog/playback. The local library and NetEase session are separate.
class QqMusicBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY changed)
    Q_PROPERTY(QString nickname READ nickname NOTIFY changed)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY changed)
    Q_PROPERTY(QString membership READ membership NOTIFY changed)
    Q_PROPERTY(QString loginChannel READ loginChannel WRITE setLoginChannel NOTIFY changed)
    Q_PROPERTY(QVariantList songs READ songs NOTIFY songsChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(QVariantMap currentPlaylist READ currentPlaylist NOTIFY changed)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY changed)
public:
    QqMusicBridge(Library *library,PlaybackFacade *facade,LyricsModel *lyrics,ai::AiRuntime *ai,OnlineArtworkCache *artwork,QObject *parent=nullptr);
    ~QqMusicBridge() override;
    bool loggedIn() const{return m_loggedIn;}
    bool busy() const{return !m_pageCache.quiet()&&(!m_requests.isEmpty()||m_aiTask!=0);}
    bool catalogLoading() const{return m_pageCache.pending();}
    QString nickname() const{return m_nickname;}
    QUrl avatarUrl() const{return m_avatarUrl;}
    QString status() const{return m_status;}
    QString qrImage() const{return m_qrImage;}
    QString membership() const{return m_membership;}
    QString loginChannel() const{return m_loginChannel;}
    QString quality() const{return m_quality;}
    QVariantList songs() const{return m_songs;}
    QVariantList playlists() const{return m_playlists;}
    QVariantList searchResults() const{return m_searchResults;}
    QVariantMap currentPlaylist() const{return m_currentPlaylist;}
    void setLoginChannel(const QString &channel);
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
    static constexpr qint64 TrackBase=OnlineMusicBridge::TrackBase*2;
signals:
    void changed();
    void songsChanged();
    void downloadReady(const QVariantMap &song,const QUrl &url,const QString &extension);
    void downloadFailed(const QVariantMap &song,const QString &error);
private:
    using JsonDone=std::function<void(const QJsonObject &)>;
    using ReplyDone=std::function<void(QNetworkReply *,const QByteArray &)>;
    Library *m_library;PlaybackFacade *m_facade;LyricsModel *m_lyrics;ai::AiRuntime *m_ai;
    OnlineArtworkCache *m_artwork;
    OnlinePageCache m_pageCache;bool m_forcePage=false;QTimer m_pageCommit;
    QNetworkAccessManager m_network;MusicCookieJar *m_cookies;
    QSet<QNetworkReply *> m_requests;
    QPointer<QNetworkReply> m_loginRequest;
    QTimer m_poll;
    QString m_nickname,m_membership,m_status=QStringLiteral("连接 QQ 音乐账号，发现更多音乐"),m_qrImage,m_qrKey;
    QUrl m_avatarUrl;
    QString m_loginChannel="qq",m_quality="hires";
    bool m_loggedIn=false,m_activated=false;
    QJsonObject m_credential;
    QVariantList m_songs,m_playlists,m_searchResults;QHash<QString,QVariantMap> m_collections;QHash<qint64,QVariantMap> m_catalog;
    QVariantMap m_currentPlaylist;
    quint64 m_loginRevision=0,m_listRevision=0,m_playRevision=0,m_aiRevision=0,m_aiTask=0;
    QString onlinePath(const QString &file) const;
    void setStatus(const QString &status);
    void trace(const QString &message) const;
    QNetworkReply *request(const QUrl &url,const QByteArray &body,const QByteArray &contentType,ReplyDone done,bool login=false,int timeout=15000);
    void cgi(const QString &module,const QString &method,const QJsonObject &param,JsonDone done,const QJsonObject &extraComm={},bool searchEnvelope=false,bool login=false);
    void pollLogin();
    void authorizeQq(const QUrl &callback,quint64 revision);
    void continueQqAuthorization(const QUrl &url,quint64 revision,int redirectsLeft);
    void exchangeLogin(const QString &code,quint64 revision);
    void acceptCredential(const QJsonObject &data,quint64 revision);
    void refreshAccount();
    void saveSession();
    void installMusicCookies();
    QString userId() const;
    void setSongs(const QJsonArray &songs);
    struct PlaylistLoad;
    void fetchPlaylistPage(const std::shared_ptr<PlaylistLoad> &load);
    QVariantMap song(const QJsonObject &object) const;
    void requestSource(qint64 id);
    void loadExtras(qint64 id);
    void rankWithAi();
    bool beginPage(const QString &key);
    void openCollection(const QString &kind,const QString &id);
    void loadPlaylist(const QString &id);
};
}
