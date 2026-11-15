// SPDX-License-Identifier: GPL-3.0-only
// Relaxer Qt adaptation, QUEQIAN, 2026-11-13. See REFERENCES.md.
#pragma once
#include "online-music-bridge.h"
#include "online-artwork-cache.h"
namespace relaxer {
class BilibiliBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY changed)
    Q_PROPERTY(QString nickname READ nickname NOTIFY changed)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY changed)
    Q_PROPERTY(QString membership READ membership NOTIFY changed)
    Q_PROPERTY(bool vip READ vip NOTIFY changed)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY changed)
    Q_PROPERTY(QString audioQuality READ audioQuality WRITE setAudioQuality NOTIFY changed)
    Q_PROPERTY(QVariantList songs READ songs NOTIFY songsChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(QVariantMap currentPlaylist READ currentPlaylist NOTIFY changed)
    Q_PROPERTY(QVariantList videoParts READ videoParts NOTIFY changed)
    Q_PROPERTY(int currentPart READ currentPart NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(bool needsVerification READ needsVerification NOTIFY changed)
public:
    BilibiliBridge(Library *library,PlaybackFacade *facade,OnlineArtworkCache *artwork,QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr);
    ~BilibiliBridge() override;
    bool loggedIn()const{return m_loggedIn;}
    bool busy()const{return !m_requests.isEmpty();}
    bool catalogLoading()const{return m_catalogLoading;}
    QString nickname()const{return m_nickname;}
    QUrl avatarUrl()const{return m_avatar;}
    QString membership()const{return m_loggedIn?(m_vip?QStringLiteral("大会员"):QStringLiteral("普通账号")):QString();}
    bool vip()const{return m_loggedIn&&m_vip;}
    QString qrImage()const{return m_qrImage;}
    QString status()const{return m_status;}
    QString quality()const{return m_quality;}
    void setQuality(const QString &value);
    QString audioQuality()const{return m_audioQuality;}
    void setAudioQuality(const QString &value);
    QVariantList songs()const{return m_songs;}
    QVariantList playlists()const{return m_playlists;}
    QVariantList searchResults()const{return m_searchResults;}
    QVariantMap currentPlaylist()const{return m_currentPlaylist;}
    QVariantList videoParts()const;
    int currentPart()const;
    Q_INVOKABLE bool selectPart(int page);
    bool hasMore()const{return m_hasMore;}
    bool needsVerification()const{return m_needsVerification;}
    Q_INVOKABLE void activate();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void stopLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void search(const QString &query,int category=0);
    Q_INVOKABLE void showPage(const QString &kind,const QString &argument,bool refresh=false);
    Q_INVOKABLE void openPlaylist(const QString &id);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void playSong(const QString &id);
    void playSongs(const QStringList &ids,const QString &start);
    void savePinnedPlaylist(const QString &id){m_pageCache.savePinned("playlist:"+id);}
    Q_INVOKABLE bool batchSongs(const QStringList &ids,const QString &action);
    Q_INVOKABLE void cancelRecommendation(){}
    static QVariantList normalizeVideos(const QJsonArray &videos);
    static QPair<QJsonObject,QJsonObject> selectDash(const QJsonObject &data,int ceiling,const QString &audioQuality);
    static QList<QUrl> playbackUrls(const QJsonObject &stream);
    static QUrl signedUrl(const QString &path,QMap<QString,QString> query,const QString &keys,qint64 timestamp);
signals:
    void playlistMissing(const QString &id);
    void changed();
    void songsChanged();
    void loginCompleted();
private:
    using Done=std::function<void(const QJsonObject &,bool)>;
    Library *m_library;PlaybackFacade *m_facade;OnlineArtworkCache *m_artwork;
    QNetworkAccessManager m_network;QNetworkAccessManager *m_transport;MusicCookieJar *m_cookies;
    QHash<QNetworkReply *,QString> m_requests;
    QTimer m_poll;
    bool m_activated=false,m_loggedIn=false,m_vip=false,m_catalogLoading=false,m_pollPending=false,m_hasMore=false,m_needsVerification=false;
    QString m_nickname,m_userId,m_qrImage,m_qrKey,m_keys,m_status=QStringLiteral("搜索 B站视频，登录后查看收藏夹");
    QString m_kind="featured",m_argument,m_quality="80",m_audioQuality="high";
    OnlinePageCache m_pageCache;
    qint64 m_keysTime=0;
    QUrl m_avatar;
    int m_page=1;
    quint64 m_loginRevision=0,m_listRevision=0,m_playRevision=0;
    QVariantList m_songs,m_playlists,m_searchResults;QVariantMap m_currentPlaylist;
    QHash<QString,QVariantMap> m_folders,m_users;
    void loadFolderCover(const QString &id,quint64 revision);
    QHash<QString,QVariantMap> m_catalog;
    void get(const QUrl &url,const QString &purpose,Done done);
    void signedGet(const QString &path,QMap<QString,QString> query,const QString &purpose,Done done);
    void abort(const QString &purpose);
    void refreshAccount(bool fromLogin=false);
    void pollLogin();
    void saveSession();
    QString sessionPath()const;
    void readPage(bool append=false);
    void cachePage();
    void registerVideos(QVariantList rows,bool append=false);
    QVector<Track> tracksFor(const QStringList &ids);
    void requestSource(qint64 id);
    void status(const QString &text){m_status=text;emit changed();}
};
}
