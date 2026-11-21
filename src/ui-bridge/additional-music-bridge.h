#pragma once
#include "online-music-bridge.h"
#include "online-artwork-cache.h"
#include <QSet>

namespace relaxer {
// Two native platform adapters share transport/lifetime management. No external
// API relay, Node runtime, or access to another application's account files.
class AdditionalMusicBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(QString membership READ membership NOTIFY changed)
    Q_PROPERTY(bool vip READ vip NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool catalogLoading READ catalogLoading NOTIFY changed)
    Q_PROPERTY(QString nickname READ nickname NOTIFY changed)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY changed)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY changed)
    Q_PROPERTY(QVariantList songs READ songs NOTIFY songsChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(QVariantMap currentPlaylist READ currentPlaylist NOTIFY changed)
public:
    enum Platform { Kugou, Qishui };
    AdditionalMusicBridge(Platform platform,Library *library,PlaybackFacade *facade,LyricsModel *lyrics,
                          OnlineArtworkCache *artwork,QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr);
    ~AdditionalMusicBridge() override;
    bool loggedIn()const{return m_loggedIn;}
    bool busy()const{return !m_requests.isEmpty();}
    bool catalogLoading()const{return m_catalogLoading;}
    QString nickname()const{return m_nickname;}
    QString membership()const{return m_membership;}
    bool vip()const{return m_loggedIn&&!m_membership.isEmpty()&&m_membership!=QStringLiteral("普通账号");}
    QUrl avatarUrl()const{return m_avatar;}
    QString status()const{return m_status;}
    QString qrImage()const{return m_qrImage;}
    QString quality()const{return m_quality;}
    void setQuality(const QString &quality);
    QVariantList songs()const{return m_songs;}
    QVariantList playlists()const{return m_playlists;}
    QVariantList searchResults()const{return {};}
    QVariantMap currentPlaylist()const{return m_currentPlaylist;}
    Q_INVOKABLE void activate();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void stopLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void showPage(const QString &kind,const QString &argument,bool refresh=false);
    Q_INVOKABLE void search(const QString &query,int category=0);
    Q_INVOKABLE void openPlaylist(const QString &id);
    Q_INVOKABLE void playSong(const QString &id);
    Q_INVOKABLE void cancelRecommendation();
    void playSongs(const QStringList &ids,const QString &start);
    Q_INVOKABLE bool batchSongs(const QStringList &ids,const QString &action);
    void savePinnedPlaylist(const QString &id);
    void resolveDownload(const QVariantMap &song,const QString &quality);
    static QVariantList normalizeSongs(Platform platform,const QJsonValue &response);
    static QVariantList normalizePlaylists(Platform platform,const QJsonValue &response);
    static QUrl kugouUrl(const QString &host,const QString &path,QMap<QString,QString> parameters,
                        const QString &device,const QJsonObject &credential,qint64 time,bool web=false,
                        const QByteArray &body={},bool audioKey=false);
signals:
    void playlistMissing(const QString &id);
    void changed();
    void songsChanged();
    void loginCompleted();
    void downloadReady(const QVariantMap &song,const QUrl &url,const QString &extension);
    void downloadFailed(const QVariantMap &song,const QString &error);
private:
    void trimCatalog();
    QVector<Track> tracksFor(const QStringList &ids);
    using Done=std::function<void(const QJsonObject &,bool)>;
    Platform m_platform;
    Library *m_library;PlaybackFacade *m_facade;LyricsModel *m_lyrics;OnlineArtworkCache *m_artwork;
    OnlinePageCache m_pageCache;
    QNetworkAccessManager m_network;QNetworkAccessManager *m_transport;MusicCookieJar *m_cookies;
    QHash<QNetworkReply *,QString> m_requests;
    QTimer m_poll;
    bool m_loggedIn=false,m_activated=false,m_catalogLoading=false,m_pollPending=false;
    QString m_membership;
    QString m_device,m_nickname,m_status,m_qrImage,m_qrKey,m_quality="standard";
    QString m_kind="featured",m_argument;
    QString m_sodaClientToken;
    QUrl m_avatar;QJsonObject m_credential;
    QVariantList m_songs,m_playlists;QVariantMap m_currentPlaylist;
    QHash<QString,QVariantMap> m_catalog;QHash<qint64,QString> m_internal;
    QHash<QString,qint64> m_identifiers;qint64 m_nextInternal=1;
    QHash<QString,QVariantMap> m_playlistCatalog;
    quint64 m_loginRevision=0,m_listRevision=0,m_playRevision=0;
    QString provider()const{return m_platform==Kugou?"kugou":"qishui";}
    QString name()const{return m_platform==Kugou?QStringLiteral("酷狗音乐"):QStringLiteral("汽水音乐");}
    QString sessionPath()const;
    void saveSession();void refreshAccount();void pollLogin();
    void status(const QString &text){m_status=text;emit changed();}
    void abort(const QString &purpose);
    void request(const QUrl &url,const QByteArray &body,const QByteArray &type,const QString &purpose,Done done,
                 const QMap<QByteArray,QByteArray> &headers={},const QByteArray &responseKey={});
    void kg(const QString &host,const QString &path,QMap<QString,QString> query,const QString &purpose,Done done,
            bool web=false,const QByteArray &body={},bool audioKey=false,bool post=false,bool retryDevice=true);
    void soda(const QString &host,const QString &path,QMap<QString,QString> query,const QString &purpose,Done done,
              const QByteArray &body={},bool form=false);
    void sodaPlatform(const QString &path,QJsonObject body,const QString &purpose,Done done,bool account=false,int retry=0);
    void sodaTicket(const QJsonObject &access,const QJsonObject &guard,quint64 revision,const QString &expectedUser={});
    void sodaProfile(QJsonObject credential,quint64 revision,const QString &expectedUser={});
    void registerKugou(const QString &purpose,Done done);
    void readSodaPlaylists(quint64 revision,int collection=0,const QString &cursor={},QVariantList rows={},QSet<QString> cursors={});
    void requestSodaPlaylist(const QString &id,Done done,bool legacy,bool fallback);
    void loadKugouCovers(quint64 revision,const QStringList &ids,int offset=0);
    void setSongs(const QJsonValue &response);
    void registerSongs(QVariantList rows);
    void readCatalog(const QString &kind,const QString &argument);
    void cachePage();
    void requestSource(qint64 id);
    void resolveSource(const QVariantMap &song,const QString &quality,Done done);
    void resolveSodaAudio(const QJsonObject &response,const QString &quality,Done done,bool preview=false);
};
}
