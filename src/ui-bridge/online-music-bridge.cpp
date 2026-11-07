#include "online-music-bridge.h"
#include "online-recommendation.h"
#include "online-artwork-cache.h"
#include "../../third_party/qrcodegen/qrcodegen.hpp"
#include <QBuffer>
#include <QDir>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QPainter>
#include <QSaveFile>
#include <QUrlQuery>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#endif

namespace relaxer {
namespace {
QString stringId(const QJsonValue &value){return value.toVariant().toString();}
QString artworkUrl(const QString &source,int edge){
    if(source.isEmpty())return {};QUrl url(source);
    // NetEase alternates HTTP/HTTPS and equivalent picture CDN hosts for the
    // same image. Keep that variation out of catalog comparisons and caches.
    static const QRegularExpression pictureHost("^p[0-9]+\\.music\\.126\\.net$");
    if(pictureHost.match(url.host()).hasMatch()){url.setScheme("https");url.setHost("p1.music.126.net");}
    QUrlQuery query(url);query.removeAllQueryItems("param");
    query.addQueryItem("param",QString("%1y%1").arg(edge));url.setQuery(query);return url.toString();
}
// NetEase's desktop QR endpoints use eapi, including the client header in the
// encrypted payload. The legacy plain /api path can return 8821 after approval.
QByteArray desktopLoginForm(const QString &path,QJsonObject data,const QJsonObject &header){
#ifdef Q_OS_WIN
    data.insert("header",header);data.insert("e_r",false);
    const auto uri=path.toUtf8(),json=QJsonDocument(data).toJson(QJsonDocument::Compact);
    const auto digest=QCryptographicHash::hash("nobody"+uri+"use"+json+"md5forencrypt",QCryptographicHash::Md5).toHex();
    auto plain=uri+"-36cd479b6b5-"+json+"-36cd479b6b5-"+digest;
    QByteArray secret("e82ckenh8dichen8");
    struct Cipher {
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE key=nullptr;
        ~Cipher(){if(key)BCryptDestroyKey(key);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    } cipher;
    if(BCryptOpenAlgorithmProvider(&cipher.algorithm,BCRYPT_AES_ALGORITHM,nullptr,0)<0
       ||BCryptSetProperty(cipher.algorithm,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_ECB)),sizeof(BCRYPT_CHAIN_MODE_ECB),0)<0
       ||BCryptGenerateSymmetricKey(cipher.algorithm,&cipher.key,nullptr,0,reinterpret_cast<PUCHAR>(secret.data()),ULONG(secret.size()),0)<0)return {};
    ULONG size=0;
    if(BCryptEncrypt(cipher.key,reinterpret_cast<PUCHAR>(plain.data()),ULONG(plain.size()),nullptr,nullptr,0,nullptr,0,&size,BCRYPT_BLOCK_PADDING)<0)return {};
    QByteArray encrypted(int(size),Qt::Uninitialized);
    if(BCryptEncrypt(cipher.key,reinterpret_cast<PUCHAR>(plain.data()),ULONG(plain.size()),nullptr,nullptr,0,reinterpret_cast<PUCHAR>(encrypted.data()),size,&size,BCRYPT_BLOCK_PADDING)<0)return {};
    encrypted.resize(size);return "params="+encrypted.toHex();
#else
    Q_UNUSED(path) Q_UNUSED(data) Q_UNUSED(header) return {};
#endif
}
QByteArray protectSession(const QByteArray &bytes,bool decrypt){
#ifdef Q_OS_WIN
    DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE *>(const_cast<char *>(bytes.constData()))},output{};
    const bool ok=decrypt?CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)
                         :CryptProtectData(&input,L"Relaxer Netease session",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output);
    if(!ok)return {};
    const QByteArray result(reinterpret_cast<char *>(output.pbData),int(output.cbData));
    SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return result;
#else
    Q_UNUSED(bytes) Q_UNUSED(decrypt) return {};
#endif
}
QString qrImageFor(const QString &text){
    const auto code=qrcodegen::QrCode::encodeText(text.toUtf8().constData(),qrcodegen::QrCode::Ecc::MEDIUM);
    const int cell=5,border=4,size=(code.getSize()+border*2)*cell;
    QImage image(size,size,QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);
    for(int y=0;y<code.getSize();++y)for(int x=0;x<code.getSize();++x)
        if(code.getModule(x,y))painter.fillRect((x+border)*cell,(y+border)*cell,cell,cell,Qt::black);
    painter.end();QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");
    return "data:image/png;base64,"+QString::fromLatin1(png.toBase64());
}
}
QByteArray MusicCookieJar::serialize() const {
    QJsonArray values;for(const auto &cookie:allCookies())values.append(QString::fromLatin1(cookie.toRawForm().toBase64()));
    return QJsonDocument(values).toJson(QJsonDocument::Compact);
}
void MusicCookieJar::restore(const QByteArray &bytes){
    QList<QNetworkCookie> values;for(const auto &value:QJsonDocument::fromJson(bytes).array())
        values.append(QNetworkCookie::parseCookies(QByteArray::fromBase64(value.toString().toLatin1())));
    setAllCookies(values);
}
bool MusicCookieJar::hasSession() const {
    for(const auto &cookie:allCookies())if(cookie.name()=="MUSIC_U"&&!cookie.value().isEmpty())return true;
    return false;
}
QByteArray MusicCookieJar::protect(const QByteArray &bytes,bool decrypt){return protectSession(bytes,decrypt);}
OnlineMusicBridge::OnlineMusicBridge(Library *library,PlaybackFacade *facade,LyricsModel *lyrics,ai::AiRuntime *ai,OnlineArtworkCache *artwork,QObject *parent)
    :QObject(parent),m_library(library),m_facade(facade),m_lyrics(lyrics),m_ai(ai),m_artwork(artwork),m_pageCache(library,"netease"),m_network(this),m_cookies(new MusicCookieJar(&m_network)){
    m_network.setCookieJar(m_cookies);m_poll.setInterval(1800);
    m_pageCommit.setSingleShot(true);connect(this,&OnlineMusicBridge::changed,this,[this]{if(m_pageCache.pending())m_pageCommit.start(0);});
    connect(&m_pageCommit,&QTimer::timeout,this,[this]{
        if(!m_pageCache.pending()||!m_requests.isEmpty()||m_aiTask)return;
        const auto before=m_pageCache.before();const bool metadataChanged=m_pageCache.quiet()&&before.value("songs").toList()==m_songs&&before.value("playlist").toMap()!=m_currentPlaylist;
        m_pageCache.finish({{"songs",m_songs},{"playlists",m_playlists},{"searchResults",m_searchResults},{"playlist",m_currentPlaylist},{"status",m_status}});if(metadataChanged)emit songsChanged();emit changed();
    });
    const auto restoreQuality=[this]{auto saved=m_library->setting("online.quality","hires").toString();if(!QStringList{"standard","exhigh","lossless","hires"}.contains(saved))saved="hires";if(m_quality!=saved){m_quality=saved;emit changed();}};
    restoreQuality();connect(library,&Library::settingsUpdated,this,[restoreQuality](const QStringList &keys){if(keys.contains("online.quality"))restoreQuality();});
    connect(&m_poll,&QTimer::timeout,this,&OnlineMusicBridge::pollLogin);
    connect(facade,&PlaybackFacade::onlineSourceRequested,this,&OnlineMusicBridge::requestSource);
    connect(m_ai->requests(),&ai::AiRequestCoordinator::completionFinished,this,[this](quint64 task,const QString &,const ai::ChatCompletionResult &result){
        if(task!=m_aiTask)return;m_aiTask=0;emit changed();if(m_aiRevision!=m_listRevision)return;
        if(!result.ok()){setStatus("AI 推荐失败："+result.error.message);return;}
        auto text=result.content.trimmed();if(text.startsWith("```")){text=text.mid(text.indexOf('\n')+1);text=text.left(text.lastIndexOf("```"));}
        const auto selected=QJsonDocument::fromJson(text.toUtf8()).object().value("recommendations").toArray();
        QVariantList ranked;QSet<QString> used;
        for(const auto &value:selected){const auto object=value.toObject();const auto id=stringId(object.value("id"));if(used.contains(id))continue;
            for(const auto &candidate:m_songs){auto row=candidate.toMap();if(row.value("id").toString()!=id)continue;
                row["reason"]=object.value("reason").toString();ranked.append(row);used.insert(id);break;}
            if(ranked.size()>=10)break;
        }
        if(ranked.isEmpty()){setStatus("AI 未返回有效候选，请重试");return;}
        m_songs=ranked;emit songsChanged();
        QSaveFile cache(onlinePath("recommendations.json"));if(cache.open(QIODevice::WriteOnly)){cache.write(QJsonDocument(QJsonArray::fromVariantList(m_songs)).toJson(QJsonDocument::Compact));cache.commit();}
        setStatus(QString("AI 已结合本地收听偏好推荐 %1 首歌曲").arg(m_songs.size()));
    });
}
OnlineMusicBridge::~OnlineMusicBridge(){m_poll.stop();cancelRecommendation();for(auto *reply:m_requests){reply->disconnect(this);reply->abort();}}
QString OnlineMusicBridge::onlinePath(const QString &file) const {
    const auto folder=QDir(m_library->root()).filePath("Online");QDir().mkpath(folder);return QDir(folder).filePath(file);
}
void OnlineMusicBridge::setStatus(const QString &status){if(!m_pageCache.quiet())m_status=status;emit changed();}
bool OnlineMusicBridge::beginPage(const QString &key){
    QVariantMap snapshot;const bool force=m_forcePage;m_forcePage=false;const auto action=m_pageCache.begin(key,force,snapshot);
    if(!snapshot.isEmpty()){
        const bool changedSongs=m_songs!=snapshot.value("songs").toList()||m_currentPlaylist!=snapshot.value("playlist").toMap();
        m_songs=snapshot.value("songs").toList();m_playlists=snapshot.value("playlists").toList();m_searchResults=snapshot.value("searchResults").toList();m_currentPlaylist=snapshot.value("playlist").toMap();m_status=snapshot.value("status").toString();
        for(const auto &v:m_songs){const auto row=v.toMap();m_catalog.insert(row.value("id").toLongLong(),row);}if(changedSongs||key.startsWith("playlist:"))emit songsChanged();emit this->changed();
    }
    if(action==OnlinePageCache::Cached){++m_listRevision;return true;}return action==OnlinePageCache::Pending;
}
void OnlineMusicBridge::showPage(const QString &kind,const QString &argument,bool refresh){
    m_forcePage=refresh;
    if(kind=="featured")recommendedPlaylists();else if(kind=="mine")loadPlaylists();else if(kind=="daily")dailyRecommendations();else if(kind=="ai")recommend();else if(kind=="search")search(argument);else if(kind=="search-artists")search(argument,1);else if(kind=="search-albums")search(argument,2);else if(kind=="search-playlists")search(argument,3);else if(kind=="artist"||kind=="album")openCollection(kind,argument);else if(kind=="playlist")openPlaylist(argument);m_forcePage=false;
}
void OnlineMusicBridge::traceLogin(const QString &message) const {
    if(!qEnvironmentVariableIsSet("RELAXER_NETEASE_LOGIN_TRACE"))return;
    QFile file(onlinePath("login-trace.txt"));if(file.open(QIODevice::WriteOnly|QIODevice::Append))
        file.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+" "+message+"\n").toUtf8());
}
QNetworkReply *OnlineMusicBridge::post(const QString &path,const QJsonObject &data,std::function<void(const QJsonObject &)> done){
    const bool desktopLogin=path.startsWith("/api/login/qrcode/")||path=="/api/song/enhance/download/url/v1";
    QNetworkRequest request(QUrl(desktopLogin?"https://interfacepc.music.163.com/eapi/"+path.mid(5):"https://music.163.com"+path));request.setTransferTimeout(15000);
    request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Referer","https://music.163.com/");
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QByteArray form;for(auto it=data.begin();it!=data.end();++it){
        if(!form.isEmpty())form+='&';const auto value=it.value().isBool()?(it.value().toBool()?QString("true"):QString("false")):it.value().toVariant().toString();
        form+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(value);
    }
    if(desktopLogin){
        const auto now=QDateTime::currentMSecsSinceEpoch();
        QJsonObject header{{"os","pc"},{"appver","3.1.29.205117"},{"osver","Microsoft-Windows-10-Professional-build-19045-64bit"},
            {"deviceId",m_deviceId},{"channel","netease"},{"versioncode","140"},{"buildver",QString::number(now/1000)},
            {"requestId",QString::number(now)+"_"+QString::number(QRandomGenerator::global()->bounded(10000)).rightJustified(4,'0')}};
        auto cookies=m_cookies->cookiesForUrl(request.url());
        for(const auto &cookie:cookies)if(cookie.name()=="MUSIC_U"||cookie.name()=="__csrf"||cookie.name()=="NMTID")
            header.insert(QString::fromLatin1(cookie.name()),QString::fromLatin1(cookie.value()));
        for(auto it=header.begin();it!=header.end();++it){
            const auto name=it.key().toLatin1();for(qsizetype i=cookies.size()-1;i>=0;--i)if(cookies[i].name()==name)cookies.removeAt(i);
            cookies.append(QNetworkCookie(name,QUrl::toPercentEncoding(it.value().toString())));
        }
        request.setHeader(QNetworkRequest::CookieHeader,QVariant::fromValue(cookies));
        request.setRawHeader("User-Agent","Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Safari/537.36 Chrome/91.0.4472.164 NeteaseMusicDesktop/3.1.29.205117");
        form=desktopLoginForm(path,data,header);
        if(form.isEmpty()){setStatus("网易云登录请求生成失败，请重试");done({});return nullptr;}
    }
    auto *reply=m_network.post(request,form);m_requests.insert(reply);emit changed();
    connect(reply,&QNetworkReply::finished,this,[this,reply,path,done=std::move(done)]{
        m_requests.remove(reply);const auto object=QJsonDocument::fromJson(reply->readAll()).object();const bool ok=reply->error()==QNetworkReply::NoError;
        if(path.contains("/login/qrcode/")||path=="/api/w/nuser/account/get")
            traceLogin(QString("response %1 http=%2 network=%3 code=%4 session=%5").arg(path).arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(int(reply->error())).arg(object.value("code").toInt()).arg(m_cookies->hasSession()));
        reply->deleteLater();emit changed();if(!ok||object.isEmpty()){setStatus("网易云连接失败，请稍后重试");done({});return;}
        done(object);
    });
    return reply;
}
void OnlineMusicBridge::activate(){
    if(m_activated)return;m_activated=true;restoreSession();
    QFile cache(onlinePath("recommendations.json"));if(cache.open(QIODevice::ReadOnly)){m_songs=QJsonDocument::fromJson(cache.readAll()).array().toVariantList();for(auto &v:m_songs){auto row=v.toMap();const auto cover=row.value("cover").toString();row["cover"]=artworkUrl(cover,1024);row["thumbnail"]=artworkUrl(cover,128);v=row;m_catalog.insert(row.value("id").toLongLong(),row);}emit songsChanged();}
    if(m_cookies->hasSession())refreshAccount();
}
void OnlineMusicBridge::saveSession(){
    const auto bytes=protectSession(m_cookies->serialize(),false);if(bytes.isEmpty()){setStatus("已登录，本次登录状态未能保存在本机");return;}
    QSaveFile file(onlinePath("netease-session.bin"));if(file.open(QIODevice::WriteOnly)){file.write(bytes);file.commit();}
}
void OnlineMusicBridge::restoreSession(){QFile file(onlinePath("netease-session.bin"));if(file.open(QIODevice::ReadOnly))m_cookies->restore(protectSession(file.readAll(),true));}
void OnlineMusicBridge::refreshAccount(){
    const auto revision=m_loginRevision;
    post("/api/w/nuser/account/get",{},[this,revision](const QJsonObject &object){
        if(revision!=m_loginRevision)return;
        const auto profile=object.value("profile").toObject();m_loggedIn=!profile.isEmpty();m_userId=stringId(profile.value("userId"));m_nickname=profile.value("nickname").toString();m_avatarUrl=QUrl(profile.value("avatarUrl").toString());
        setStatus(m_loggedIn?"已登录网易云 · "+m_nickname:"请使用网易云 App 扫码登录");
    });
}
void OnlineMusicBridge::startLogin(){
    stopLogin();const auto revision=m_loginRevision;m_qrImage.clear();setStatus("正在生成登录二维码…");
    if(m_deviceId.isEmpty()){
        QFile file(onlinePath("netease-device-id.txt"));if(file.open(QIODevice::ReadOnly))m_deviceId=QString::fromUtf8(file.readAll()).trimmed();
        if(m_deviceId.isEmpty()){m_deviceId=QUuid::createUuid().toString(QUuid::WithoutBraces);QSaveFile saved(onlinePath("netease-device-id.txt"));if(saved.open(QIODevice::WriteOnly)){saved.write(m_deviceId.toUtf8());saved.commit();}}
    }
    traceLogin(QString("start revision=%1").arg(revision));
    post("/api/login/qrcode/unikey",{{"type",3}},[this,revision](const QJsonObject &object){
        if(revision!=m_loginRevision)return;m_qrKey=object.value("unikey").toString();
        if(m_qrKey.isEmpty()){setStatus("二维码生成失败，请重试");return;}
        m_qrImage=qrImageFor("https://music.163.com/login?codekey="+m_qrKey);setStatus("请用网易云 App 扫码，并在手机上确认登录");m_poll.start();
    });
}
void OnlineMusicBridge::stopLogin(){
    ++m_loginRevision;m_poll.stop();m_qrKey.clear();
    if(m_loginPoll){auto reply=m_loginPoll;m_loginPoll.clear();reply->abort();}
    traceLogin(QString("stop revision=%1").arg(m_loginRevision));
}
void OnlineMusicBridge::pollLogin(){
    if(m_qrKey.isEmpty()||m_loginPoll)return;const auto revision=m_loginRevision;
    m_loginPoll=post("/api/login/qrcode/client/login",{{"key",m_qrKey},{"type",3}},[this,revision](const QJsonObject &object){
        if(revision!=m_loginRevision)return;m_loginPoll.clear();const int code=object.value("code").toInt();
        if(code==803){m_poll.stop();if(!m_cookies->hasSession()){setStatus("网易云已确认授权，但未收到登录会话，请重新扫码");return;}saveSession();refreshAccount();}
        else if(code==801)setStatus("请用网易云 App 扫码，并在手机上确认登录");
        else if(code==802)setStatus("已扫码，请在手机上确认登录");
        else if(code==800){m_poll.stop();setStatus("二维码已过期，请刷新二维码");}
        else{m_poll.stop();if(code==8821)setStatus("网易云拒绝了本次扫码登录（8821），请稍后重试或使用其他登录方式");
            else if(!object.isEmpty())setStatus(QString("网易云扫码登录未完成（%1），请刷新二维码重试").arg(code));}
    });
}
void OnlineMusicBridge::logout(){
    m_pageCache.clearPrivate();
    stopLogin();++m_playRevision;++m_listRevision;cancelRecommendation();m_cookies->restore({});QFile::remove(onlinePath("netease-session.bin"));
    m_loggedIn=false;m_userId.clear();m_nickname.clear();m_avatarUrl.clear();m_qrImage.clear();m_playlists.clear();
    if(m_facade->onlineActive()&&m_facade->current().hash.startsWith("netease:"))m_facade->stopOnline();setStatus("已退出网易云账号");
}
QVariantMap OnlineMusicBridge::song(const QJsonObject &object) const {
    const auto value=object.contains("song")?object.value("song").toObject():object;
    const auto album=value.value(value.contains("al")?"al":"album").toObject();
    QStringList artists;for(const auto &v:value.value(value.contains("ar")?"ar":"artists").toArray())artists.append(v.toObject().value("name").toString());
    return {{"id",stringId(value.value("id"))},{"title",value.value("name").toString()},{"artist",artists.join(" / ")},
            {"album",album.value("name").toString()},{"cover",artworkUrl(album.value("picUrl").toString(),1024)},
            {"thumbnail",artworkUrl(album.value("picUrl").toString(),128)},
            {"duration",value.value(value.contains("dt")?"dt":"duration").toVariant()},{"reason",QString()}};
}
void OnlineMusicBridge::setSongs(const QJsonArray &songs){
    QVariantList next;QSet<QString> ids;for(const auto &value:songs){auto row=song(value.toObject());const auto id=row.value("id").toString();
        if(id.isEmpty()||ids.contains(id))continue;ids.insert(id);m_catalog.insert(id.toLongLong(),row);next.append(row);}
    if(next==m_songs)return;m_songs=next;emit songsChanged();
}
void OnlineMusicBridge::search(const QString &query,int category){
    if(category<0||category>3||query.trimmed().isEmpty()||beginPage("search:"+QString::number(category)+":"+query.trimmed()))return;
    if(query.trimmed().isEmpty())return;m_currentPlaylist.clear();cancelRecommendation();const auto revision=++m_listRevision;setStatus("正在搜索网易云…");
    m_searchResults.clear();emit changed();
    const int types[]{1,100,10,1000};
    post("/api/search/get/web",{{"s",query.trimmed()},{"type",types[category]},{"limit",50},{"offset",0}},[this,revision,category](const QJsonObject &object){
        if(revision!=m_listRevision)return;if(object.value("code").toInt()!=200)return;
        if(category>0){
            const QString keys[]{"","artists","albums","playlists"},kinds[]{"","artist","album","playlist"};
            for(const auto &value:object.value("result").toObject().value(keys[category]).toArray()){
                const auto item=value.toObject();const auto id=stringId(item.value("id"));
                const auto cover=item.value(category==3?"coverImgUrl":"picUrl").toString();
                const QVariantMap row{{"id",id},{"name",item.value("name").toString()},{"cover",artworkUrl(cover,400)},
                    {"count",item.value(category==3?"trackCount":"musicSize").toInt()},{"provider","netease"},{"kind",kinds[category]}};
                if(id.isEmpty())continue;m_searchResults.append(row);m_collections.insert(kinds[category]+":"+id,row);
            }
            setStatus(QString("找到 %1 个%2").arg(m_searchResults.size()).arg(category==1?"歌手":category==2?"专辑":"歌单"));return;
        }
        const auto found=object.value("result").toObject().value("songs").toArray();QJsonArray missing;
        for(const auto &value:found){const auto track=value.toObject();if(song(track).value("cover").toString().isEmpty())missing.append(QJsonObject{{"id",track.value("id")}});}
        if(missing.isEmpty()){setSongs(found);setStatus(QString("找到 %1 首歌曲").arg(m_songs.size()));return;}
        // Search omits album pictures. Fill the batch before exposing playable
        // rows, so the queue already knows its artwork when a row is clicked.
        post("/api/v3/song/detail",{{"c",QString::fromUtf8(QJsonDocument(missing).toJson(QJsonDocument::Compact))}},[this,revision,found](const QJsonObject &details){
            if(revision!=m_listRevision)return;if(details.value("code").toInt()!=200){setStatus("网易云歌曲详情读取失败，请重新搜索");return;}QHash<QString,QJsonObject> complete;
            for(const auto &value:details.value("songs").toArray()){const auto track=value.toObject();complete.insert(stringId(track.value("id")),track);}
            QJsonArray merged;for(const auto &value:found){const auto track=value.toObject();merged.append(complete.value(stringId(track.value("id")),track));}
            setSongs(merged);setStatus(QString("找到 %1 首歌曲").arg(m_songs.size()));
        });
    });
}
void OnlineMusicBridge::openCollection(const QString &kind,const QString &id){
    if(beginPage(kind+":"+id))return;
    cancelRecommendation();const auto revision=++m_listRevision;
    m_currentPlaylist=m_collections.value(kind+":"+id);m_currentPlaylist["id"]=id;m_currentPlaylist["kind"]=kind;
    setSongs({});setStatus(kind=="artist"?"正在读取歌手歌曲…":"正在读取专辑…");
    const auto path=kind=="artist"?"/api/v1/artist/"+id:"/api/v1/album/"+id;
    post(path,{},[this,revision,kind](const QJsonObject &result){
        if(revision!=m_listRevision)return;
        if(result.value("code").toInt()!=200){setStatus("歌曲读取失败，请重试");return;}
        setSongs(result.value(kind=="artist"?"hotSongs":"songs").toArray());m_currentPlaylist["count"]=m_songs.size();
        setStatus(QString("%1 · %2 首歌曲").arg(kind=="artist"?"歌手热门歌曲":"专辑").arg(m_songs.size()));
    });
}
void OnlineMusicBridge::dailyRecommendations(){
    if(m_loggedIn&&beginPage("daily"))return;
    m_currentPlaylist.clear();
    if(!m_loggedIn){setStatus("请先登录网易云账号，再获取每日推荐");return;}
    cancelRecommendation();const auto revision=++m_listRevision;setStatus("正在获取每日推荐…");
    post("/api/v3/discovery/recommend/songs",{},[this,revision](const QJsonObject &object){
        if(revision!=m_listRevision)return;if(object.value("code").toInt()!=200){setStatus("每日推荐获取失败，请刷新登录状态");return;}
        setSongs(object.value("data").toObject().value("dailySongs").toArray());setStatus(QString("网易云每日推荐 · %1 首").arg(m_songs.size()));
    });
}
void OnlineMusicBridge::loadPlaylists(){
    if(!m_loggedIn){setStatus("请先登录网易云账号");return;}
    if(beginPage("mine"))return;cancelRecommendation();const auto revision=++m_listRevision;m_currentPlaylist.clear();setSongs({});
    post("/api/user/playlist",{{"uid",m_userId},{"limit",100},{"offset",0}},[this,revision](const QJsonObject &object){
        if(revision!=m_listRevision)return;
        m_playlists.clear();for(const auto &value:object.value("playlist").toArray()){const auto p=value.toObject();m_playlists.append(QVariantMap{{"id",stringId(p.value("id"))},{"name",p.value("name").toString()},{"cover",artworkUrl(p.value("coverImgUrl").toString(),300)},{"count",p.value("trackCount").toInt()},{"provider","netease"}});}
        setStatus(QString("读取到 %1 个网易云歌单").arg(m_playlists.size()));
    });
}
void OnlineMusicBridge::recommendedPlaylists(){
    if(beginPage("featured"))return;
    cancelRecommendation();const auto revision=++m_listRevision;m_currentPlaylist.clear();setSongs({});setStatus("正在读取网易云推荐歌单…");
    post("/api/personalized/playlist",{{"limit",24}},[this,revision](const QJsonObject &object){
        if(revision!=m_listRevision)return;m_playlists.clear();for(const auto &value:object.value("result").toArray()){const auto p=value.toObject();m_playlists.append(QVariantMap{{"id",stringId(p.value("id"))},{"name",p.value("name").toString()},{"cover",artworkUrl(p.value("picUrl").toString(),300)},{"count",p.value("trackCount").toInt()},{"provider","netease"}});}setStatus(m_playlists.isEmpty()?"暂未取得推荐歌单，请重试":"网易云推荐歌单");
    });
}
void OnlineMusicBridge::downloadSong(const QString &id){auto row=m_catalog.value(id.toLongLong());row["provider"]="netease";if(!row.value("id").toString().isEmpty())resolveDownload(row,m_quality);}
void OnlineMusicBridge::resolveDownload(const QVariantMap &song,const QString &quality){
    post("/api/song/enhance/download/url/v1",{{"id",song.value("id").toString()},{"level",quality},{"immerseType","c51"}},[this,row=song](const QJsonObject &object)mutable{
        const auto data=object.value("data").toObject();const QUrl url(data.value("url").toString());
        if(url.isEmpty()||data.value("freeTrialInfo").isObject()){const QString error=object.isEmpty()?"网易云连接失败，请稍后重试":"当前网易云账号未取得这首歌的完整下载权限";if(!row.contains("_downloadTask"))setStatus(error);emit downloadFailed(row,error);return;}
        row["provider"]="netease";row["key"]="netease:"+row.value("id").toString();auto type=data.value("type").toString();if(type.isEmpty())type="mp3";emit downloadReady(row,url,type);
    });
}
struct OnlineMusicBridge::PlaylistLoad {
    quint64 revision=0;QStringList ids;QHash<QString,QJsonObject> tracks;qsizetype next=0;int expected=0;
};
void OnlineMusicBridge::openPlaylist(const QString &id){
    if(id.toLongLong()<=0||beginPage("playlist:"+id))return;
    if(id.toLongLong()<=0)return;cancelRecommendation();const auto revision=++m_listRevision;if(!m_pageCache.quiet())setSongs({});setStatus("正在读取歌单…");
    post("/api/v6/playlist/detail",{{"id",id},{"n",100},{"s",0}},[this,revision](const QJsonObject &object){
        if(revision!=m_listRevision)return;
        if(object.value("code").toInt()!=200){setStatus("网易云歌单读取失败，请稍后重试");return;}
        const auto playlist=object.value("playlist").toObject();auto load=std::make_shared<PlaylistLoad>();load->revision=revision;load->expected=playlist.value("trackCount").toInt();
        m_currentPlaylist={{"id",stringId(playlist.value("id"))},{"provider","netease"},{"name",playlist.value("name").toString()},{"cover",artworkUrl(playlist.value("coverImgUrl").toString(),300)},{"count",load->expected}};
        QSet<QString> seen;
        for(const auto &value:playlist.value("trackIds").toArray()){
            const auto id=stringId(value.toObject().value("id"));if(id.toLongLong()>0&&!seen.contains(id)){seen.insert(id);load->ids.append(id);}
        }
        const bool haveIds=!load->ids.isEmpty();
        for(const auto &value:playlist.value("tracks").toArray()){
            const auto track=value.toObject();const auto id=stringId(track.value("id"));if(id.toLongLong()<=0)continue;load->tracks.insert(id,track);
            if(!haveIds&&!seen.contains(id)){seen.insert(id);load->ids.append(id);}
        }
        fetchPlaylistBatch(load);
    });
}
void OnlineMusicBridge::fetchPlaylistBatch(const std::shared_ptr<PlaylistLoad> &load){
    if(load->revision!=m_listRevision)return;QJsonArray batch;
    while(load->next<load->ids.size()&&batch.size()<200){const auto id=load->ids[load->next++];if(!load->tracks.contains(id))batch.append(QJsonObject{{"id",id.toLongLong()}});}
    if(batch.isEmpty()){finishPlaylist(load);return;}
    setStatus(QString("正在读取歌单 · %1 / %2 首").arg(load->tracks.size()).arg(load->ids.size()));
    post("/api/v3/song/detail",{{"c",QString::fromUtf8(QJsonDocument(batch).toJson(QJsonDocument::Compact))}},[this,load](const QJsonObject &object){
        if(load->revision!=m_listRevision)return;if(object.value("code").toInt()!=200){finishPlaylist(load,true);return;}
        for(const auto &value:object.value("songs").toArray()){const auto track=value.toObject();load->tracks.insert(stringId(track.value("id")),track);}
        fetchPlaylistBatch(load);
    });
}
void OnlineMusicBridge::finishPlaylist(const std::shared_ptr<PlaylistLoad> &load,bool failed){
    if(failed&&m_pageCache.quiet()){m_currentPlaylist=m_pageCache.before().value("playlist").toMap();setStatus("歌单读取未完成");return;}
    if(load->revision!=m_listRevision)return;QJsonArray songs;int unavailable=0;
    for(const auto &id:load->ids){
        if(load->tracks.contains(id))songs.append(load->tracks.value(id));
        else if(!failed){songs.append(QJsonObject{{"id",id.toLongLong()},{"name","暂不可用的歌曲"}});++unavailable;}
    }
    setSongs(songs);
    if(failed)setStatus(QString("已读取 %1 首，后续歌曲读取失败，请重新打开歌单重试").arg(m_songs.size()));
    else if(load->expected>load->ids.size())setStatus(QString("歌单 · %1 首（平台仅返回这些可读取曲目）").arg(m_songs.size()));
    else setStatus(QString("歌单 · %1 首歌曲").arg(m_songs.size())+(unavailable?QString(" · %1 首暂不可用").arg(unavailable):QString()));
}
void OnlineMusicBridge::setQuality(const QString &quality){
    if(quality==m_quality||!QStringList{"standard","exhigh","lossless","hires"}.contains(quality))return;if(!m_library->setSetting("online.quality",quality))setStatus("音质设置未能保存，请重试");
}
void OnlineMusicBridge::playSong(const QString &id){
    if(id.toLongLong()<=0)return;
    if(!m_catalog.contains(id.toLongLong())){
        const auto revision=++m_listRevision;
        post("/api/v3/song/detail",{{"c",QString("[{\"id\":%1}]").arg(id.toLongLong())}},[this,id,revision](const QJsonObject &object){
            if(revision!=m_listRevision)return;const auto list=object.value("songs").toArray();
            if(list.isEmpty()){setStatus("未找到这首网易云歌曲");return;}setSongs(list);playSong(id);
        });return;
    }
    QStringList ids;for(const auto &value:m_songs)ids.append(value.toMap().value("id").toString());playSongs(ids,id);
}
void OnlineMusicBridge::playSongs(const QStringList &ids,const QString &start){
    QVector<Track> tracks;
    for(const auto &id:ids){const auto row=m_catalog.value(id.toLongLong());if(row.isEmpty())continue;Track track;track.id=TrackBase+row.value("id").toLongLong();track.title=row.value("title").toString();track.artist=row.value("artist").toString();track.album=row.value("album").toString();track.duration=row.value("duration").toLongLong();track.hash="netease:"+row.value("id").toString();
        const auto cover=row.value("cover").toString();m_artwork->prepare(track,QUrl(artworkUrl(cover,128)),QUrl(artworkUrl(cover,1024)),onlinePath(QString("cover-%1.png").arg(row.value("id").toString())));tracks.append(track);}
    m_facade->playOnline(tracks,TrackBase+start.toLongLong());
}
void OnlineMusicBridge::requestSource(qint64 internalId){
    if(internalId<TrackBase||internalId>=TrackBase*2)return;
    const auto revision=++m_playRevision;const auto id=QString::number(internalId-TrackBase);setStatus("正在获取网易云播放地址…");
    post("/api/song/enhance/player/url/v1",{{"ids","["+id+"]"},{"level",m_quality},{"encodeType","flac"}},[this,internalId,revision](const QJsonObject &object){
        if(revision!=m_playRevision||!m_facade->onlineActive()||m_facade->current().id!=internalId)return;
        const auto data=object.value("data").toArray();const auto entry=data.isEmpty()?QJsonObject():data.first().toObject();const QUrl url(entry.value("url").toString());
        if(url.isEmpty()){const QString message="当前账号暂不能播放这首歌，请检查登录状态或歌曲权限";setStatus(message);m_facade->failOnlineSource(internalId,message);return;}
        const bool trial=entry.value("freeTrialInfo").isObject();setStatus(trial?"当前账号获得的是试听片段":"正在播放 · 网易云音乐");
        m_facade->setOnlineSource(internalId,url);loadExtras(internalId);
    });
}
void OnlineMusicBridge::loadExtras(qint64 internalId){
    const auto id=internalId-TrackBase;
    post("/api/song/lyric",{{"id",QString::number(id)},{"lv",-1},{"tv",-1}},[this,internalId](const QJsonObject &object){
        m_lyrics->setOnlineLyrics(internalId,object.value("lrc").toObject().value("lyric").toString().toUtf8()+"\n"+object.value("tlyric").toObject().value("lyric").toString().toUtf8());
    });
}
void OnlineMusicBridge::toggleFavorite(qint64 internalId){
    if(internalId<TrackBase||internalId>=TrackBase*2)return;
    if(!m_loggedIn){setStatus("登录网易云后可收藏在线歌曲");return;}auto track=m_facade->track(internalId);
    post("/api/radio/like",{{"trackId",QString::number(internalId-TrackBase)},{"like",!track.favorite},{"alg","itembased"},{"time",25}},[this,track](const QJsonObject &object)mutable{
        if(object.value("code").toInt()!=200){setStatus("网易云收藏操作未成功");return;}track.favorite=!track.favorite;m_facade->updateOnlineTrack(track);setStatus(track.favorite?"已加入网易云喜欢的音乐":"已取消网易云喜欢");
    });
}
void OnlineMusicBridge::cancelRecommendation(){if(m_aiTask){m_pageCache.cancelAI();const auto task=m_aiTask;m_aiTask=0;m_ai->requests()->cancel(task);emit changed();}}
void OnlineMusicBridge::recommend(){
    if(beginPage("ai"))return;
    m_currentPlaylist.clear();
    cancelRecommendation();const auto error=m_ai->validateConfiguration(m_ai->configuration().defaults);
    if(!error.isEmpty()){setStatus("请先在 AI 设置中配置模型："+error);return;}
    const auto revision=++m_listRevision;setStatus("正在获取在线候选…");
    post(m_loggedIn?"/api/v3/discovery/recommend/songs":"/api/personalized/newsong",{{"limit",50}},[this,revision](const QJsonObject &object){
        if(revision!=m_listRevision)return;const auto candidates=m_loggedIn?object.value("data").toObject().value("dailySongs").toArray():object.value("result").toArray();
        if(candidates.isEmpty()){setStatus("未能获取推荐候选，请登录后重试");return;}setSongs(candidates);rankWithAi();
    });
}
void OnlineMusicBridge::rankWithAi(){
    const auto config=m_ai->configuration().defaults;const auto *model=m_ai->model(config.providerId,config.modelId);
    if(!model){setStatus("AI 模型配置不可用，请检查 AI 设置");return;}
    const auto request=recommendationRequest(config,*model,m_library->tracks(),m_songs);
    if(request.messages.isEmpty()){setStatus("所选 AI 模型的上下文空间不足以生成推荐，请更换模型");return;}
    m_aiRevision=m_listRevision;m_aiTask=m_ai->requests()->complete(config.providerId,request);setStatus("AI 正在结合本地收听记录挑选歌曲…");
}
}
