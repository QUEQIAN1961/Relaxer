// SPDX-License-Identifier: GPL-3.0-only
// Interface flows adapted for Relaxer from wiliwili by xfangfang / contributors:
// mine_api.cpp (Created by fang on 2022/7/26), search_api.cpp
// (Created by fang on 2022/8/2), video_detail_api.cpp, api.h
// (Created by fang on 2022/5/1), and util/wbi.cpp.
// wbi.cpp: "Created for wiliwili based on bilibili-API-collect documentation".
// Upstream commit 88e5876bea9502d06f46a8656e3530684d3aaf7d, GPLv3.
// Modified by QUEQIAN, 2026-11-13: Qt transport, signing, isolated credentials,
// cancellation, catalog/queue adapters, Qt playback and selected-part resolution.
// No cpr/borealis/mpv, upstream UI, artwork, fonts or bundled binaries copied.
// See REFERENCES.md and docs/third-party/wiliwili/LICENSE.
#include "bilibili-bridge.h"
#include "online-catalog-memory.h"
#include <QScopeGuard>
#include "disk-cache.h"
#include "../../third_party/qrcodegen/qrcodegen.hpp"
#include <QBuffer>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QPainter>
#include <QSaveFile>
#include <QTextDocumentFragment>
#include <QUrlQuery>
#include <algorithm>

namespace relaxer {
namespace {
QString identifier(const QJsonValue &value){return value.isString()?value.toString():QString::number(qint64(value.toDouble()));}
QString imageUrl(QString value){if(value.startsWith("//"))value.prepend("https:");if(value.startsWith("http:"))value.replace(0,5,"https:");return value;}
qint64 videoDuration(const QJsonValue &value){
    if(value.isDouble())return qRound64(value.toDouble()*1000);
    qint64 seconds=0;for(const auto &part:value.toString().split(':'))seconds=seconds*60+part.toLongLong();return seconds*1000;
}
qint64 internalId(const QString &bvid){const auto bytes=QCryptographicHash::hash(bvid.toUtf8(),QCryptographicHash::Sha256);quint64 id=0;for(int i=0;i<6;++i)id=(id<<8)|quint8(bytes[i]);return 5000000000000LL+qint64(id);}
QString qrData(const QString &text){
    const auto qr=qrcodegen::QrCode::encodeText(text.toUtf8().constData(),qrcodegen::QrCode::Ecc::MEDIUM);
    const int cell=5,border=4;QImage image((qr.getSize()+border*2)*cell,(qr.getSize()+border*2)*cell,QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);
    for(int y=0;y<qr.getSize();++y)for(int x=0;x<qr.getSize();++x)if(qr.getModule(x,y))painter.fillRect((x+border)*cell,(y+border)*cell,cell,cell,Qt::black);
    painter.end();QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");return "data:image/png;base64,"+QString::fromLatin1(png.toBase64());
}
QUrl api(const QString &path,const QMap<QString,QString> &query={}){QUrl url("https://api.bilibili.com"+path);QUrlQuery parameters;for(auto it=query.begin();it!=query.end();++it)parameters.addQueryItem(it.key(),it.value());url.setQuery(parameters);return url;}
}
QUrl BilibiliBridge::signedUrl(const QString &path,QMap<QString,QString> query,const QString &keys,qint64 timestamp){
    // Protocol permutation used by wiliwili's GPLv3 WBI module. No upstream
    // documentation prose or its third-party MD5/cpr/pystring implementation.
    static constexpr int permutation[]={46,47,18,2,53,8,23,32,15,50,10,31,58,3,45,35,27,43,5,49,33,9,42,19,29,28,14,39,12,38,41,13};
    if(keys.size()!=64)return {};
    QByteArray mixin;for(const int index:permutation)mixin+=keys[index].toLatin1();
    query["wts"]=QString::number(timestamp);QByteArray encoded;
    for(auto it=query.begin();it!=query.end();++it){
        auto value=it.value();for(const auto ch:QString("!'()*"))value.remove(ch);
        if(!encoded.isEmpty())encoded+='&';encoded+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(value);
    }
    const auto signature=QCryptographicHash::hash(encoded+mixin,QCryptographicHash::Md5).toHex();
    QUrl url("https://api.bilibili.com"+path);url.setQuery(QString::fromLatin1(encoded+"&w_rid="+signature));return url;
}
QPair<QJsonObject,QJsonObject> BilibiliBridge::selectDash(const QJsonObject &data,int ceiling,const QString &audioQuality){
    const auto dash=data.value("dash").toObject();QJsonObject video,audio;
    for(const auto &entry:dash.value("video").toArray()){
        const auto row=entry.toObject();const int quality=row.value("id").toInt();if(quality>ceiling||playbackUrls(row).isEmpty())continue;
        if(video.isEmpty()||quality>video.value("id").toInt()||(quality==video.value("id").toInt()&&row.value("codecid").toInt()==7&&video.value("codecid").toInt()!=7))video=row;
    }
    for(const auto &entry:dash.value("audio").toArray()){
        const auto row=entry.toObject();if(playbackUrls(row).isEmpty())continue;
        if(audio.isEmpty()||(audioQuality=="standard"?row.value("bandwidth").toInt()<audio.value("bandwidth").toInt():row.value("bandwidth").toInt()>audio.value("bandwidth").toInt()))audio=row;
    }
    if(audioQuality=="hires"){
        const auto flac=dash.value("flac").toObject().value("audio");
        if(flac.isObject()&&!playbackUrls(flac.toObject()).isEmpty())audio=flac.toObject();
        else if(flac.isArray())for(const auto &entry:flac.toArray())if(!playbackUrls(entry.toObject()).isEmpty()){audio=entry.toObject();break;}
    }
    return {video,audio};
}
QList<QUrl> BilibiliBridge::playbackUrls(const QJsonObject &stream){
    QList<QUrl> urls;QStringList sources{stream.value("url").toString(stream.value("baseUrl").toString(stream.value("base_url").toString()))};
    for(const auto &value:stream.value("backup_url").toArray(stream.value("backupUrl").toArray()))sources.append(value.toString());
    for(const auto &source:sources){
        QUrl url(source);if(url.scheme().isEmpty()&&!url.host().isEmpty())url.setScheme("https");
        if(!url.isValid()||!QStringList{"http","https"}.contains(url.scheme())||url.host().isEmpty())continue;
        const auto host=url.host().toLower();
        if(url.scheme()=="http"&&(host.endsWith(".bilivideo.com")||host.endsWith(".bilivideo.cn")))url.setScheme("https");
        if(!urls.contains(url))urls.append(url);
    }
    // The regional cn-* edge returned by Bilibili can time out while its
    // standard upos CDN mirrors work. Keep every usable candidate as fallback.
    const auto standard=[](const QUrl &url){const auto host=url.host().toLower();return host.startsWith("upos-")&&(host.endsWith(".bilivideo.com")||host.endsWith(".bilivideo.cn"));};
    std::stable_sort(urls.begin(),urls.end(),[&](const auto &a,const auto &b){return standard(a)&&!standard(b);});
    return urls;
}
QVariantList BilibiliBridge::normalizeVideos(const QJsonArray &videos){
    QVariantList rows;
    for(const auto &value:videos){const auto video=value.toObject();const auto bvid=video.value("bvid").toString();if(bvid.isEmpty())continue;
        const auto upper=video.value("upper").toObject();const auto title=QTextDocumentFragment::fromHtml(video.value("title").toString()).toPlainText();
        const auto cover=imageUrl(video.value("pic").toString(video.value("cover").toString()));
        rows.append(QVariantMap{{"id",bvid},{"bvid",bvid},{"internalId",QString::number(internalId(bvid))},{"provider","bilibili"},{"title",title},{"artist",video.value("author").toString(upper.value("name").toString())},{"album",QStringLiteral("B站视频")},{"duration",videoDuration(video.contains("duration")?video.value("duration"):video.value("length"))},{"cover",cover},{"thumbnail",cover},{"reason",QString()},{"requiresSubscription",false}});
    }return rows;
}
BilibiliBridge::BilibiliBridge(Library *library,PlaybackFacade *facade,OnlineArtworkCache *artwork,QObject *parent,QNetworkAccessManager *transport)
    :QObject(parent),m_library(library),m_facade(facade),m_artwork(artwork),m_network(this),m_transport(transport?transport:&m_network),m_cookies(new MusicCookieJar(&m_network)),m_pageCache(library,"bilibili"){
    m_quality=library->setting("bilibili.quality","80").toString();m_audioQuality=library->setting("bilibili.audioQuality","high").toString();
    m_network.setCookieJar(m_cookies);m_poll.setInterval(1800);connect(&m_poll,&QTimer::timeout,this,&BilibiliBridge::pollLogin);
    connect(facade,&PlaybackFacade::onlineSourceRequested,this,&BilibiliBridge::requestSource);
    connect(facade,&PlaybackFacade::onlineTrackUpdated,this,[this](qint64 id){if(m_facade->current().id==id)emit changed();});
    connect(facade,&PlaybackFacade::trackChanged,this,[this]{++m_partsRevision;m_partsKey.clear();m_partsLoading=false;emit changed();ensureVideoParts();});
}
BilibiliBridge::~BilibiliBridge(){m_poll.stop();for(auto *reply:m_requests.keys()){reply->disconnect(this);reply->abort();}}
QString BilibiliBridge::sessionPath()const{const auto folder=QDir(m_library->root()).filePath("Online");QDir().mkpath(folder);return QDir(folder).filePath("bilibili-session.bin");}
void BilibiliBridge::saveSession(){const auto protectedBytes=MusicCookieJar::protect(m_cookies->serialize(),false);if(protectedBytes.isEmpty()){status("B站登录有效，但未能保存登录状态");return;}QSaveFile file(sessionPath());if(!file.open(QIODevice::WriteOnly)||file.write(protectedBytes)!=protectedBytes.size()||!file.commit())status("B站登录状态保存失败");}
void BilibiliBridge::activate(){if(m_activated)return;m_activated=true;QFile file(sessionPath());if(file.open(QIODevice::ReadOnly)){m_cookies->restore(MusicCookieJar::protect(file.readAll(),true));refreshAccount();}}
void BilibiliBridge::get(const QUrl &url,const QString &purpose,Done done){
    QNetworkRequest request(url);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Referer","https://www.bilibili.com/");
    auto *reply=m_transport->get(request);m_requests.insert(reply,purpose);emit changed();
    connect(reply,&QNetworkReply::finished,this,[this,reply,done=std::move(done)]{
        const auto body=reply->readAll();const auto object=QJsonDocument::fromJson(body).object();const bool ok=reply->error()==QNetworkReply::NoError&&!object.isEmpty();
        m_requests.remove(reply);reply->deleteLater();done(object,ok);emit changed();
    });
}
void BilibiliBridge::abort(const QString &purpose){for(auto *reply:m_requests.keys())if(m_requests.value(reply)==purpose){m_requests.remove(reply);reply->abort();}}
void BilibiliBridge::signedGet(const QString &path,QMap<QString,QString> query,const QString &purpose,Done done){
    const auto revision=m_listRevision;
    const auto send=[this,path,query,purpose,done,revision]{if(purpose=="catalog"&&revision!=m_listRevision)return;const auto url=signedUrl(path,query,m_keys,QDateTime::currentSecsSinceEpoch());if(url.isEmpty()){done({},false);return;}get(url,purpose,done);};
    if(m_keys.size()==64&&QDateTime::currentSecsSinceEpoch()-m_keysTime<3600){send();return;}
    get(api("/x/web-interface/nav"),purpose,[this,send,done](const QJsonObject &object,bool ok){
        const auto images=object.value("data").toObject().value("wbi_img").toObject();
        m_keys=QFileInfo(QUrl(images.value("img_url").toString()).path()).completeBaseName()+QFileInfo(QUrl(images.value("sub_url").toString()).path()).completeBaseName();
        if(!ok||m_keys.size()!=64){done({},false);return;}m_keysTime=QDateTime::currentSecsSinceEpoch();send();
    });
}
void BilibiliBridge::refreshAccount(bool fromLogin){
    const auto revision=m_loginRevision;get(api("/x/web-interface/nav"),"account",[this,revision,fromLogin](const QJsonObject &object,bool ok){
        if(revision!=m_loginRevision)return;
        const auto data=object.value("data").toObject();m_loggedIn=ok&&object.value("code").toInt(-1)==0&&data.value("isLogin").toBool();
        if(!ok){status("B站账号暂时连接失败，可稍后重试");return;}
        m_nickname=m_loggedIn?data.value("uname").toString():QString();m_userId=m_loggedIn?identifier(data.value("mid")):QString();m_avatar=m_loggedIn?QUrl(imageUrl(data.value("face").toString())):QUrl();
        m_vip=m_loggedIn&&data.value("vipStatus").toInt()==1;
        status(m_loggedIn?"已登录 B站 · "+m_nickname:"请使用哔哩哔哩 App 扫码登录");
        if(m_loggedIn&&fromLogin){m_pageCache.clearPrivate();saveSession();emit loginCompleted();}
        // The default favorites view may have opened before session restoration
        // completed. Load it after login without replacing another active view.
        if(m_loggedIn&&m_kind=="mine"&&!m_catalogLoading)showPage("mine",QString());
    });
}
void BilibiliBridge::startLogin(){
    activate();stopLogin();const auto revision=m_loginRevision;m_qrImage.clear();status("正在生成 B站登录二维码…");
    get(QUrl("https://passport.bilibili.com/x/passport-login/web/qrcode/generate"),"login",[this,revision](const QJsonObject &object,bool ok){
        if(revision!=m_loginRevision)return;const auto data=object.value("data").toObject();const auto url=QUrl(data.value("url").toString());m_qrKey=data.value("qrcode_key").toString();
        if(!ok||object.value("code").toInt(-1)!=0||url.host().isEmpty()||m_qrKey.isEmpty()){status("B站二维码生成失败，请重试");return;}
        m_qrImage=qrData(url.toString());status("请使用哔哩哔哩 App 扫码确认");m_poll.start();
    });
}
void BilibiliBridge::stopLogin(){++m_loginRevision;m_poll.stop();m_pollPending=false;m_qrKey.clear();abort("login");}
void BilibiliBridge::pollLogin(){
    if(m_qrKey.isEmpty()||m_pollPending)return;m_pollPending=true;const auto revision=m_loginRevision;
    QUrl url("https://passport.bilibili.com/x/passport-login/web/qrcode/poll");QUrlQuery query;query.addQueryItem("qrcode_key",m_qrKey);url.setQuery(query);
    get(url,"login",[this,revision](const QJsonObject &object,bool ok){
        if(revision!=m_loginRevision)return;m_pollPending=false;
        const int code=object.value("data").toObject().value("code").toInt(-1);
        if(!ok){status("B站登录连接中断，正在等待重试");return;}
        if(code==0){m_poll.stop();m_qrKey.clear();m_qrImage.clear();refreshAccount(true);}
        else if(code==86038){m_poll.stop();status("B站二维码已过期，请刷新");}
        else if(code==86090)status("已扫码，请在手机确认登录");
    });
}
void BilibiliBridge::logout(){m_pageCache.clearPrivate();stopLogin();abort("account");m_cookies->restore({});QFile::remove(sessionPath());m_loggedIn=false;m_vip=false;m_nickname.clear();m_userId.clear();m_avatar=QUrl{};++m_listRevision;abort("catalog");abort("covers");m_catalogLoading=false;m_songs.clear();m_playlists.clear();m_currentPlaylist.clear();m_hasMore=false;emit songsChanged();status("已退出 B站账号");}
void BilibiliBridge::setQuality(const QString &value){
    if(!QStringList{"16","32","64","74","80","112","116","120","125","126","127"}.contains(value)||m_quality==value)return;
    if(m_library->setSetting("bilibili.quality",value)){m_quality=value;emit changed();if(m_facade->onlineActive()&&m_facade->current().hash.startsWith("bilibili:"))m_facade->refreshOnlineSource();}
}
void BilibiliBridge::setAudioQuality(const QString &value){
    if(!QStringList{"standard","high","hires"}.contains(value)||m_audioQuality==value)return;
    if(m_library->setSetting("bilibili.audioQuality",value)){m_audioQuality=value;emit changed();if(m_facade->onlineActive()&&m_facade->current().hash.startsWith("bilibili:"))m_facade->refreshOnlineSource();}
}
void BilibiliBridge::showPage(const QString &kind,const QString &argument,bool refresh){
    activate();if(!refresh&&m_catalogLoading&&m_kind==kind&&m_argument==argument)return;
    m_pageCache.cancelPending();
    QVariantMap cached;const auto action=kind=="mine"&&!m_loggedIn?OnlinePageCache::Fetch:
        m_pageCache.begin(kind+(argument.isEmpty()?QString():":"+argument),refresh,cached);
    ++m_listRevision;abort("catalog");abort("covers");m_kind=kind;m_argument=argument;m_page=1;m_hasMore=false;m_needsVerification=false;m_currentPlaylist.clear();m_songs.clear();m_playlists.clear();m_searchResults.clear();
    if(!cached.isEmpty()){
        m_currentPlaylist=cached.value("playlist").toMap();m_playlists=cached.value("playlists").toList();m_searchResults=cached.value("searchResults").toList();
        registerVideos(cached.value("songs").toList());m_page=cached.value("page",1).toInt();m_hasMore=cached.value("hasMore").toBool();m_needsVerification=cached.value("needsVerification").toBool();
        if(action==OnlinePageCache::Cached){m_catalogLoading=false;trimCatalog();emit songsChanged();status(cached.value("status").toString());return;}
        m_page=1;
    }
    if(kind=="playlist"){
        if(m_currentPlaylist.isEmpty())m_currentPlaylist=m_folders.value(argument,QVariantMap{{"id",argument},{"name",QStringLiteral("B站收藏夹")},{"count",0},{"provider","bilibili"}});
    }else if(kind=="user")m_currentPlaylist=m_users.value(argument);
    emit songsChanged();
    if(kind=="featured"){m_catalogLoading=false;status("搜索 B站视频或用户，登录后查看收藏夹");cachePage();return;}
    if(kind=="mine"&&!m_loggedIn){m_catalogLoading=false;status("登录 B站后可读取自己的收藏夹");return;}
    readPage();
}
void BilibiliBridge::search(const QString &query,int category){showPage(category==1?"search-users":"search",query);}
void BilibiliBridge::openPlaylist(const QString &id){showPage("playlist",id);}
void BilibiliBridge::loadMore(){if(!m_hasMore||m_catalogLoading)return;++m_page;readPage(true);}
void BilibiliBridge::cachePage(){
    if(m_catalogLoading||(m_kind=="mine"&&!m_loggedIn))return;
    trimCatalog();
    m_pageCache.finish({{"songs",m_songs},{"playlists",m_playlists},{"searchResults",m_searchResults},{"playlist",m_currentPlaylist},
        {"page",m_page},{"hasMore",m_hasMore},{"needsVerification",m_needsVerification},{"status",m_status}});
}
void BilibiliBridge::loadFolderCover(const QString &id,quint64 revision){
    get(api("/x/v3/fav/resource/list",{{"media_id",id},{"pn","1"},{"ps","1"},{"order","mtime"},{"platform","web"}}),"covers",[this,id,revision](const QJsonObject &object,bool ok){
        if(revision!=m_listRevision||!ok||object.value("code").toInt(-1)!=0)return;
        const auto data=object.value("data").toObject();auto cover=imageUrl(data.value("info").toObject().value("cover").toString());
        if(cover.isEmpty())for(const auto &entry:data.value("medias").toArray()){cover=imageUrl(entry.toObject().value("cover").toString());if(!cover.isEmpty())break;}
        if(cover.isEmpty())return;
        auto row=m_folders.value(id);row["cover"]=cover;m_folders.insert(id,row);
        for(auto &entry:m_playlists)if(entry.toMap().value("id").toString()==id){entry=row;break;}
        cachePage();emit changed();
    });
}
void BilibiliBridge::readPage(bool append){
    m_needsVerification=false;m_catalogLoading=true;const auto revision=m_listRevision;status("正在读取 B站内容…");
    if(m_kind=="mine"){
        get(api("/x/v3/fav/folder/created/list-all",{{"up_mid",m_userId}}),"catalog",[this,revision](const QJsonObject &object,bool ok){
            if(revision!=m_listRevision)return;m_catalogLoading=false;
            const auto commit=qScopeGuard([this]{cachePage();});
            if(!ok||object.value("code").toInt(-1)!=0){status("B站收藏夹读取失败，请刷新或重新登录");return;}
            for(const auto &entry:object.value("data").toObject().value("list").toArray()){const auto row=entry.toObject();m_playlists.append(QVariantMap{{"id",identifier(row.value("id"))},{"name",row.value("title").toString()},{"cover",imageUrl(row.value("cover").toString())},{"count",row.value("media_count").toInt()},{"provider","bilibili"}});}
            for(const auto &entry:m_playlists){const auto row=entry.toMap();const auto id=row.value("id").toString();m_folders.insert(id,row);if(row.value("cover").toString().isEmpty()&&row.value("count").toInt()>0)loadFolderCover(id,revision);}
            status(QString("我的收藏夹 · %1 个").arg(m_playlists.size()));
        });return;
    }
    const auto done=[this,revision,append](const QJsonObject &object,bool ok){
        if(revision!=m_listRevision)return;m_catalogLoading=false;
        const auto commit=qScopeGuard([this]{cachePage();});
        if(m_kind=="playlist"&&playlistReportedMissing(object)){status("B站收藏夹已不存在");emit playlistMissing(m_argument);return;}
        if(!ok||object.value("code").toInt(-1)!=0){if(append)--m_page;status("B站内容读取失败，请刷新重试");return;}
        const auto data=object.value("data").toObject();const bool collection=m_kind=="playlist";
        if(data.contains("v_voucher")){m_needsVerification=true;if(append)--m_page;status("B站要求安全验证。请先登录后重试，或在 B站官网查看结果。");return;}
        const bool userSearch=m_kind=="search-users",userVideos=m_kind=="user";
        const auto entries=collection?data.value("medias").toArray():userVideos?data.value("list").toObject().value("vlist").toArray():data.value("result").toArray();
        if(userSearch){
            if(!append)m_searchResults.clear();QSet<QString> seen;for(const auto &row:m_searchResults)seen.insert(row.toMap().value("id").toString());
            for(const auto &entry:entries){const auto user=entry.toObject();const auto id=identifier(user.value("mid"));if(id=="0"||id.isEmpty())continue;
                const QVariantMap row{{"id",id},{"name",user.value("uname").toString()},{"cover",imageUrl(user.value("upic").toString(user.value("uface").toString()))},{"count",user.value("videos").toInt()},{"kind","user"},{"provider","bilibili"}};
                m_users.insert(id,row);if(!seen.contains(id)){m_searchResults.append(row);seen.insert(id);}
            }
        }else registerVideos(normalizeVideos(entries),append);
        m_hasMore=collection?data.value("has_more").toBool():userVideos?m_page*20<data.value("page").toObject().value("count").toInt():m_page<data.value("numPages").toInt();
        if(entries.isEmpty())m_hasMore=false;
        if(collection){const auto info=data.value("info").toObject();m_currentPlaylist["name"]=info.value("title").toString();auto cover=imageUrl(info.value("cover").toString());if(cover.isEmpty()&&m_page==1)for(const auto &entry:entries){cover=imageUrl(entry.toObject().value("cover").toString());if(!cover.isEmpty())break;}if(cover.isEmpty())cover=m_currentPlaylist.value("cover").toString();m_currentPlaylist["cover"]=cover;m_currentPlaylist["count"]=info.value("media_count").toInt();}
        if(collection)m_folders.insert(m_argument,m_currentPlaylist);
        emit songsChanged();status(userSearch?QString("B站用户 · %1 个").arg(m_searchResults.size()):QString("B站视频 · %1 个").arg(m_songs.size()));
    };
    if(m_kind=="playlist")get(api("/x/v3/fav/resource/list",{{"media_id",m_argument},{"pn",QString::number(m_page)},{"ps","20"},{"platform","web"}}),"catalog",done);
    else if(m_kind=="user")signedGet("/x/space/wbi/arc/search",{{"mid",m_argument},{"pn",QString::number(m_page)},{"ps","20"},{"order","pubdate"}},"catalog",done);
    else signedGet("/x/web-interface/wbi/search/type",{{"search_type",m_kind=="search-users"?"bili_user":"video"},{"keyword",m_argument},{"page",QString::number(m_page)},{"page_size","20"},{"order","totalrank"},{"platform","pc"},{"device","mac"},{"highlight","1"},{"single_column","0"},{"from_spmid","333.337"},{"__refresh__","true"}},"catalog",done);
}
void BilibiliBridge::registerVideos(QVariantList rows,bool append){
    if(!append)m_songs.clear();QSet<QString> existing;for(const auto &row:m_songs)existing.insert(row.toMap().value("id").toString());
    for(const auto &value:rows){const auto row=value.toMap();const auto id=row.value("id").toString();m_catalog.insert(id,row);if(!existing.contains(id)){m_songs.append(row);existing.insert(id);}}
}
QVector<Track> BilibiliBridge::tracksFor(const QStringList &ids){
    QVector<Track> tracks;for(const auto &id:ids){if(!m_catalog.contains(id))continue;const auto row=m_catalog.value(id);Track track;track.id=internalId(id);track.hash="bilibili:"+id;track.title=row.value("title").toString();track.artist=row.value("artist").toString();track.album="B站视频";track.duration=row.value("duration").toLongLong();track.cover=row.value("cover").toString();track.onlineMetadata=QJsonObject::fromVariantMap(row);
        if(m_artwork)m_artwork->prepare(track,QUrl(row.value("thumbnail").toString()),QUrl(track.cover),QDir(DiskCache::online(m_library)).filePath("bilibili-cover-"+id+".png"));tracks.append(track);
    }return tracks;
}
void BilibiliBridge::playSong(const QString &id){QStringList ids;for(const auto &row:m_songs)ids.append(row.toMap().value("id").toString());playSongs(ids,id);}
void BilibiliBridge::playSongs(const QStringList &ids,const QString &start){const auto tracks=tracksFor(ids);if(!tracks.isEmpty())m_facade->playOnline(tracks,internalId(start));}
bool BilibiliBridge::batchSongs(const QStringList &ids,const QString &action){return m_facade->batchOnline(tracksFor(ids),action);}
void BilibiliBridge::readVideoInfo(const QString &bvid,Done done){
    const bool pending=m_videoInfoPending.contains(bvid);
    m_videoInfoPending[bvid].append(std::move(done));if(pending)return;
    const auto token=++m_videoInfoSerial;m_videoInfoTokens.insert(bvid,token);
    get(api("/x/web-interface/view",{{"bvid",bvid}}),"video-info:"+bvid,[this,bvid,token](const QJsonObject &object,bool ok){
        // A cancelled A request must not consume a newer A request after A/B/A.
        if(m_videoInfoTokens.value(bvid)!=token)return;
        m_videoInfoTokens.remove(bvid);const auto callbacks=m_videoInfoPending.take(bvid);
        for(const auto &callback:callbacks)callback(object,ok);
    });
}
void BilibiliBridge::ensureVideoParts(){
    const auto track=m_facade->current();const auto bvid=currentVideo()?track.hash.mid(9):QString();
    // Retire only obsolete detail requests. This never cancels audio/media URLs.
    for(const auto &key:m_videoInfoPending.keys())if(key!=bvid){
        m_videoInfoTokens.remove(key);const auto callbacks=m_videoInfoPending.take(key);abort("video-info:"+key);
        for(const auto &callback:callbacks)callback({},false);
    }
    if(bvid.isEmpty()||m_facade->isShutdown()||!track.onlineMetadata.value("pages").toArray().isEmpty()||m_partsKey==bvid)return;
    m_partsKey=bvid;m_partsLoading=true;const auto revision=m_partsRevision;activate();
    readVideoInfo(bvid,[this,id=track.id,bvid,revision](const QJsonObject &object,bool ok){
        if(revision!=m_partsRevision||m_facade->current().id!=id||m_facade->current().hash!="bilibili:"+bvid)return;
        m_partsKey.clear();m_partsLoading=false;
        const auto data=object.value("data").toObject();const auto pages=data.value("pages").toArray();
        if(!ok||object.value("code").toInt(-1)!=0||pages.isEmpty()){emit changed();return;}
        auto current=m_facade->current();const auto requested=current.onlineMetadata.value("cid").toString();QJsonObject selected;
        for(const auto &value:pages){const auto part=value.toObject();if(requested.isEmpty()?part.value("page").toInt()==currentPart():identifier(part.value("cid"))==requested){selected=part;break;}}
        if(selected.isEmpty()&&requested.isEmpty())selected=pages.first().toObject();
        const auto title=data.value("title").toString(current.onlineMetadata.value("videoTitle").toString(current.title));
        current.onlineMetadata["pages"]=pages;current.onlineMetadata["videoTitle"]=title;current.onlineMetadata["bvid"]=bvid;
        if(!selected.isEmpty()){
            current.onlineMetadata["cid"]=identifier(selected.value("cid"));current.onlineMetadata["page"]=selected.value("page").toInt(1);
            current.title=pages.size()>1?selected.value("part").toString(title):title;
            const auto length=qint64(selected.value("duration").toDouble()*1000);if(length>0)current.duration=length;
        }
        m_facade->updateOnlineTrack(current);emit changed();
    });
}
int BilibiliBridge::currentPart()const{return m_facade->current().onlineMetadata.value("page").toInt(1);}
bool BilibiliBridge::selectPart(int page){return m_facade->selectOnlinePart(page);}
QVariantList BilibiliBridge::videoParts()const{
    const auto track=m_facade->current();
    return track.hash.startsWith("bilibili:")?track.onlineMetadata.value("pages").toArray().toVariantList():QVariantList{};
}
void BilibiliBridge::requestSource(qint64 id){
    const auto track=m_facade->track(id);if(!track.hash.startsWith("bilibili:"))return;activate();const auto revision=++m_playRevision;const auto bvid=track.hash.mid(9);
    readVideoInfo(bvid,[this,id,bvid,revision](const QJsonObject &object,bool ok){
        if(revision!=m_playRevision||!m_facade->onlineActive()||m_facade->current().id!=id)return;
        const auto data=object.value("data").toObject();const auto pages=data.value("pages").toArray();
        const auto requested=m_facade->current().onlineMetadata.value("cid").toString();QJsonObject selected;
        for(const auto &value:pages)if(requested.isEmpty()||identifier(value.toObject().value("cid"))==requested){selected=value.toObject();break;}
        const auto cid=selected.isEmpty()?QString():identifier(selected.value("cid"));
        if(!ok||object.value("code").toInt(-1)!=0||cid.isEmpty()){m_facade->failOnlineSource(id,"B站视频暂不可用或需要登录",!ok);return;}
        auto track=m_facade->track(id);const auto title=data.value("title").toString(track.onlineMetadata.value("videoTitle").toString(track.title));
        track.onlineMetadata["cid"]=cid;track.onlineMetadata["bvid"]=bvid;track.onlineMetadata["pages"]=pages;track.onlineMetadata["page"]=selected.value("page").toInt(1);track.onlineMetadata["videoTitle"]=title;
        track.title=pages.size()>1?selected.value("part").toString(title):title;const auto length=qint64(selected.value("duration").toDouble()*1000);if(length>0)track.duration=length;
        m_facade->updateOnlineTrack(track);
        signedGet("/x/player/wbi/playurl",{{"bvid",bvid},{"cid",cid},{"qn",m_quality},{"fnval","4048"},{"fnver","0"},{"fourk","1"}},"playback",[this,id,bvid,revision](const QJsonObject &object,bool ok){
            if(revision!=m_playRevision||!m_facade->onlineActive()||m_facade->current().id!=id)return;
            const auto data=object.value("data").toObject();
            const auto selection=selectDash(data,m_quality.toInt(),m_audioQuality);
            if(ok&&object.value("code").toInt(-1)==0&&(!m_facade->wantsOnlineVideo()||!selection.first.isEmpty())&&!selection.second.isEmpty()){
                auto video=selection.first,audio=selection.second;
                for(auto *track:{&video,&audio}){QJsonArray urls;for(const auto &url:playbackUrls(*track))urls.append(url.toString());track->insert("urls",urls);}
                auto current=m_facade->track(id);const auto length=qint64(data.value("timelength").toDouble());if(length>0){current.duration=length;m_facade->updateOnlineTrack(current);}
                m_facade->setOnlineDashSource(id,video,audio,{{"Referer",("https://www.bilibili.com/video/"+bvid).toUtf8()}});
                status("正在播放 · 哔哩哔哩");return;
            }
            if(ok&&object.value("code").toInt(-1)==0&&!m_facade->wantsOnlineVideo()){
                const auto message=QStringLiteral("此视频未提供独立音轨，暂不支持只音频播放；可展开并切换为视频播放");status(message);m_facade->failOnlineSource(id,message,false);return;
            }
            const auto streams=data.value("durl").toArray();
            const auto urls=streams.size()==1?playbackUrls(streams.first().toObject()):QList<QUrl>{};
            if(!ok||object.value("code").toInt(-1)!=0||urls.isEmpty()){const auto message=streams.size()>1?QStringLiteral("此视频包含多个独立分段，当前请在 B站官网播放"):QStringLiteral("当前账号没有可用的单路视频地址，请检查权限或在官网播放");status(message);m_facade->failOnlineSource(id,message,!ok);return;}
            m_facade->setOnlineSource(id,urls.first(),{{"Referer",("https://www.bilibili.com/video/"+bvid).toUtf8()}},urls.mid(1));status("正在播放 · 哔哩哔哩");
        });
    });
}
void BilibiliBridge::trimCatalog(){
    QSet<QString> songs;for(const auto &v:m_songs)songs.insert(v.toMap().value("id").toString());trimCatalogMap(m_catalog,songs);
    QSet<QString> folders;for(const auto &v:m_playlists){const auto row=v.toMap();const auto id=row.value("id").toString();folders.insert(id);m_folders.insert(id,row);}
    if(m_kind=="playlist")folders.insert(m_argument);trimCatalogMap(m_folders,folders,64);
    QSet<QString> users;for(const auto &v:m_searchResults){const auto row=v.toMap();const auto id=row.value("id").toString();users.insert(id);m_users.insert(id,row);}
    if(m_kind=="user")users.insert(m_argument);trimCatalogMap(m_users,users,64);
}

}
