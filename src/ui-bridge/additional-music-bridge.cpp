// Protocol reference: MakcRe/KuGouMusicApi (MIT, 2023 MakcRe) and
// guowenye/qishui-api (MIT, 2026 arrow), GSQZ/qishui-music-api 0.2.2
// (MIT, Copyright (c) 2026 sayqz). Modified by QUEQIAN, 2026-11-14.
// See docs/third-party/music-platforms for retained licenses and scope.
#include "additional-music-bridge.h"
#include "online-catalog-memory.h"
#include "disk-cache.h"
#include "account-membership.h"
#include "qishui-protocol.h"
#include "kugou-protocol.h"
#include "../../third_party/qrcodegen/qrcodegen.hpp"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QPainter>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QScopeGuard>
#include <QUrlQuery>
#include <QUuid>

namespace relaxer {
namespace {
QString md5(const QByteArray &bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Md5).toHex());}
QString text(const QJsonValue &v){return v.isString()?v.toString():v.isDouble()?QString::number(v.toInteger()):QString();}
QString firstText(const QJsonObject &o,std::initializer_list<const char *> keys){for(auto key:keys){const auto s=text(o.value(key));if(!s.isEmpty())return s;}return {};}
QString imageUrl(const QJsonValue &value){
    if(value.isString())return value.toString();
    if(value.isArray())return value.toArray().isEmpty()?QString():imageUrl(value.toArray().first());
    const auto o=value.toObject();const auto urls=o.value("urls").toArray();
    if(!urls.isEmpty()){
        auto url=urls.first().toString();const auto uri=o.value("uri").toString();
        if(url.endsWith('/')&&!uri.isEmpty()){
            url+=uri;const auto style=o.value("template_prefix").toString();
            if(!style.isEmpty())url+='~'+style+"-image.image";
        }
        return url;
    }
    return firstText(o,{"url","uri","template"});
}
QString kugouHash(const QJsonObject &o){
    auto hash=firstText(o,{"hash","FileHash","filehash","fileHash","Hash","audio_hash"});
    if(hash.isEmpty())hash=firstText(o.value("audio_info").toObject(),{"hash","hash_128"});
    return hash;
}
QString kugouCover(const QJsonObject &song,int pixels=800){
    // These are song/album artwork fields. Author/user pictures are not covers.
    QList<QJsonObject> objects{song.value("album_info").toObject(),song.value("albumInfo").toObject(),
        song.value("audio_info").toObject().value("trans_param").toObject(),
        song.value("audioInfo").toObject().value("transParam").toObject(),song.value("trans_param").toObject(),song};
    for(const auto &object:objects){
        auto value=firstText(object,{"sizable_cover","union_cover","album_sizable_cover","Image","image","img","pic","picUrl","cover","coverUrl","imgurl"}).trimmed();
        if(value.isEmpty())continue;value.replace("{size}",QString::number(pixels));
        if(QRegularExpression("^\\d{8,}\\.(?:jpe?g|png|webp)$",QRegularExpression::CaseInsensitiveOption).match(value).hasMatch())
            value=QString("https://c1.kgimg.com/stdmusic/%1/%2/%3").arg(pixels).arg(value.left(8),value);
        if(value.startsWith("//"))value="https:"+value;
        QUrl url(value);if(url.scheme()=="http")url.setScheme("https");
        if(url.scheme()=="https"&&!url.host().isEmpty())return url.toString();
    }
    return {};
}
QUrl urlWithQuery(const QString &host,const QString &path,const QMap<QString,QString> &query){
    QUrl url(host+path);QByteArray encoded;
    for(auto it=query.cbegin();it!=query.cend();++it){if(!encoded.isEmpty())encoded+='&';encoded+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(it.value());}
    url.setQuery(QString::fromLatin1(encoded),QUrl::StrictMode);return url;
}
QString makeQrImage(const QString &payload){
    if(payload.isEmpty())return {};
    try {
        const auto qr=qrcodegen::QrCode::encodeText(payload.toUtf8().constData(),qrcodegen::QrCode::Ecc::MEDIUM);
        const int cell=5,border=4;QImage image((qr.getSize()+2*border)*cell,(qr.getSize()+2*border)*cell,QImage::Format_RGB32);image.fill(Qt::white);
        QPainter painter(&image);for(int y=0;y<qr.getSize();++y)for(int x=0;x<qr.getSize();++x)if(qr.getModule(x,y))painter.fillRect((x+border)*cell,(y+border)*cell,cell,cell,Qt::black);
        painter.end();QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");return "data:image/png;base64,"+QString::fromLatin1(png.toBase64());
    } catch(const std::exception &){return {};}
}
void visit(const QJsonValue &value,const std::function<bool(const QJsonObject &)> &accept,int depth=0){
    if(depth>18)return;
    if(value.isArray()){for(const auto &v:value.toArray())visit(v,accept,depth+1);return;}
    if(!value.isObject())return;const auto object=value.toObject();if(accept(object))return;
    for(auto it=object.begin();it!=object.end();++it)if(it.value().isArray()||it.value().isObject())visit(it.value(),accept,depth+1);
}
QString sourceExtension(const QJsonObject &source){
    const auto format=source.value("format").toString().toLower();
    if(QStringList{"mp3","flac","m4a","ogg","wav"}.contains(format))return format;
    const auto suffix=QFileInfo(QUrl(source.value("url").toString()).path()).suffix().toLower();
    return QStringList{"mp3","flac","m4a","ogg","wav"}.contains(suffix)?suffix:QStringLiteral("mp3");
}
bool sodaMediaUrl(const QUrl &url){
    if(url.scheme()!="https"||!url.userInfo().isEmpty()||(url.port()!=-1&&url.port()!=443))return false;
    for(const auto &host:QStringList{"365yg.com","douyinvod.com","douyinscdn.com","bytefcdn.com","bytecdn.cn","byteicdn.com","ibytedtos.com",
        "pstatp.com","snssdk.com","bytedance.com","bytetos.com","volcvideo.com","volccdn.com","volces.com","volcengineapi.com","douyin.com","qishui.com","zijieapi.com","zjcdn.com"})
        if(url.host()==host||url.host().endsWith("."+host))return true;
    return false;
}
QJsonObject sodaAudio(const QJsonObject &response,const QString &quality,bool preview=false){
    // Full and preview are separate platform-issued resources. Protected
    // streams are never handed to the player as plaintext audio.
    QJsonArray candidates;QString playerInfo;bool encrypted=false,invalidUrl=false;
    qint64 clipStart=0,clipDuration=0;
    const QStringList levels{"medium","higher","highest","lossless","hi_res"};
    visit(response,[&](const QJsonObject &o){
        if(o.contains("player_info")){
            const auto player=o.value("player_info").toObject();
            const auto variant=player.value(preview?"preview":"full").toObject();
            const auto info=variant.value("video_model_info").toObject();
            if(preview){const auto duration=variant.value("duration").toObject();clipStart=qMax<qint64>(0,duration.value("start_ms").toVariant().toLongLong());clipDuration=qMax<qint64>(0,duration.value("duration_ms").toVariant().toLongLong());}
            if(playerInfo.isEmpty())playerInfo=info.value("url_player_info").toString();
            const auto value=info.value("video_model");const auto model=value.isObject()?value.toObject():QJsonDocument::fromJson(value.toString().toUtf8()).object();
            for(const auto &v:model.value("video_list").toArray())candidates.append(v);
            const auto variants=model.value("video_list").toObject();for(const auto &v:variants)candidates.append(v);
            return true;
        }
        if(o.contains("MainPlayUrl")||o.contains("BackupPlayUrl"))candidates.append(o);
        return false;
    });
    QJsonObject best,fallback;int bestRank=-1,fallbackRank=100;double bestRate=-1,fallbackRate=-1;const int wanted=quality=="hires"?4:quality=="lossless"?3:quality=="exhigh"?2:0;
    for(const auto &v:candidates){const auto item=v.toObject();const auto protection=item.value("encrypt_info").toObject();
        if(protection.value("encrypt").toVariant().toBool()||!protection.value("spade_a").toString().isEmpty()||!item.value("spade_a").toString().isEmpty()||!item.value("PlayAuth").toString().isEmpty()||item.value("Encrypt").toVariant().toBool()){encrypted=true;continue;}
        QStringList urls;
        for(const auto &key:{"main_url","MainPlayUrl","backup_url","BackupPlayUrl"}){
            auto raw=item.value(key).toString();if(raw.isEmpty())continue;
            if(!raw.startsWith("http://")&&!raw.startsWith("https://"))raw=QString::fromUtf8(QByteArray::fromBase64(raw.toLatin1(),QByteArray::AbortOnBase64DecodingErrors));
            if(!sodaMediaUrl(QUrl(raw))){invalidUrl=true;continue;}
            if(!urls.contains(raw))urls.append(raw);
        }
        if(urls.isEmpty())continue;
        const auto level=firstText(item.value("video_meta").toObject(),{"quality"});const int rank=qMax(0,levels.indexOf(level.isEmpty()?item.value("Quality").toString():level));
        const auto rate=item.value("video_meta").toObject().value("bitrate").toDouble(item.value("Bitrate").toDouble());
        const QJsonObject source{{"url",urls.first()},{"backups",QJsonArray::fromStringList(urls.mid(1))},{"format",firstText(item,{"format","codec_type"})}};
        if(rank<=wanted&&(rank>bestRank||(rank==bestRank&&rate>bestRate))){bestRank=rank;bestRate=rate;best=source;}
        if(rank<fallbackRank||(rank==fallbackRank&&rate>fallbackRate)){fallbackRank=rank;fallbackRate=rate;fallback=source;}
    }
    if(best.isEmpty())best=fallback;
    if(best.isEmpty()){
        if(!playerInfo.isEmpty())best={{"playerInfoUrl",playerInfo},{"encrypted",encrypted}};
        else best={{"encrypted",encrypted},{"failureKind",encrypted?"protected":invalidUrl?"address":"no-source"}};
    }
    best["preview"]=preview;best["previewStartMs"]=clipStart;best["previewDurationMs"]=clipDuration;
    return best;
}
QJsonObject sodaRequestFailure(const QJsonObject &response){
    auto code=response.value("err_no").toInt();if(!code)code=response.value("data").toObject().value("error_code").toInt();
    const auto http=response.value("_relaxer_http_status").toInt();
    const bool auth=code==28001003||code==28001007||code==28001008||code==10007||http==401||http==403;
    return {{"failureKind",auth?"account":http==429?"rate-limit":code?"platform":http>=500||response.value("_relaxer_network_error").toInt()!=0?"network":"invalid-response"},
        {"platformCode",code},{"httpStatus",http}};
}
QString sodaFailureMessage(const QJsonObject &source){
    const auto kind=source.value("failureKind").toString();
    if(kind=="protected"||source.value("encrypted").toBool())return QStringLiteral("汽水返回了受保护音轨，当前原生播放器暂不支持；已停止自动切歌");
    if(kind=="account")return QStringLiteral("汽水播放授权已失效，请退出账号后重新扫码登录");
    if(kind=="rate-limit")return QStringLiteral("汽水请求过于频繁，请稍后手动重试");
    if(kind=="network")return QStringLiteral("汽水音源连接失败，请检查网络后重试");
    if(kind=="address")return QStringLiteral("汽水返回的音源地址暂不支持当前原生接入；已停止自动切歌");
    const auto code=source.value("platformCode").toInt();
    return code?QString("汽水音源请求被平台拒绝（错误码 %1），请稍后重试").arg(code):QStringLiteral("汽水未返回可识别的音源；已停止自动切歌");
}
}

QVariantList AdditionalMusicBridge::normalizeSongs(Platform platform,const QJsonValue &response){
    QVariantList rows;QSet<QString> seen;
    visit(response,[&](const QJsonObject &raw){
        auto o=raw;
        if(platform==Qishui&&raw.contains("base_info")){
            const auto base=raw.value("base_info").toObject();
            if(!base.contains("duration_ms"))return false;
            o=base;const auto related=raw.value("related_info").toObject();
            o["artists"]=related.value("artist_links");o["album"]=related.value("album_link");
            auto album=o.value("album").toObject();album["url_cover"]=raw.value("display_info").toObject().value("cover_url");o["album"]=album;
            o["is_vip"]=raw.value("commerce_info").toObject().value("is_vip");
        }
        const auto id=platform==Kugou?kugouHash(o):firstText(o,{"id","track_id"});
        auto title=platform==Kugou?firstText(o,{"songname","SongName","song_name","audio_name","FileName","filename","name"}):firstText(o,{"name","trackName"});
        if(id.isEmpty()||title.isEmpty())return false;
        if(platform==Qishui&&!o.contains("artists")&&!o.contains("duration")&&!o.contains("duration_ms"))return false;
        if(seen.contains(id))return true;seen.insert(id);
        const auto album=o.value("album").toObject();QString artist,cover;
        if(platform==Kugou){artist=firstText(o,{"singername","SingerName","author_name","singer_name"});cover=kugouCover(o);}
        else{QStringList artists;for(const auto &v:o.value("artists").toArray()){const auto a=v.toObject();auto s=firstText(a,{"name","simple_display_name"});if(s.isEmpty())s=a.value("user_info").toObject().value("nickname").toString();if(!s.isEmpty())artists.append(s);}artist=artists.join(" / ");cover=imageUrl(album.value("url_cover"));if(cover.isEmpty())cover=imageUrl(album.value("cover_url"));}
        const auto duration=platform==Kugou?o.value("duration").toInteger(o.value("Duration").toInteger(o.value("time_length").toInteger()))*1000:o.value("duration_ms").toInteger(o.value("duration").toInteger());
        const bool vip=platform==Kugou?o.value("pay_type").toInt()!=0||o.value("PayType").toInt()!=0||o.value("privilege").toInt()==8||o.value("feetype").toInt()!=0:o.value("is_vip").toBool()||o.value("privilege").toObject().value("is_vip").toBool();
        rows.append(QVariantMap{{"id",id},{"title",title},{"artist",artist},{"album",platform==Kugou?firstText(o,{"album_name","AlbumName"}):album.value("name").toString()},
            {"cover",cover},{"thumbnail",platform==Kugou?kugouCover(o,240):cover},{"duration",duration},{"reason",QString()},{"requiresSubscription",vip},
            {"albumId",firstText(o,{"album_id","AlbumID"})},{"audioId",firstText(o,{"album_audio_id","mixsongid"})}});
        return true;
    });return rows;
}
QVariantList AdditionalMusicBridge::normalizePlaylists(Platform platform,const QJsonValue &response){
    QVariantList rows;QSet<QString> seen;
    visit(response,[&](const QJsonObject &raw){
        auto o=raw;bool carCollection=false;
        if(platform==Qishui&&raw.contains("base_info")){
            o=raw.value("base_info").toObject();
            if(o.contains("duration_ms")||firstText(o,{"title","name"}).isEmpty())return false;
            carCollection=true;
            const auto display=raw.value("display_info").toObject();
            o["url_cover"]=display.contains("url_cover")?display.value("url_cover"):display.value("cover_url");
            o["count_tracks"]=raw.value("resource_cnt").toObject().value("track_cnt");
            if(text(o.value("owner_id")).isEmpty())o["owner_id"]=raw.value("relation_info").toObject().value("owner").toObject().value("id");
        }
        const auto id=platform==Kugou?firstText(o,{"global_collection_id","specialid","listid"}):firstText(o,{"id","playlist_id"});
        const auto name=firstText(o,{"title","name","specialname","listname"});
        if(id.isEmpty()||name.isEmpty()||(platform==Qishui&&!carCollection&&!o.contains("count_tracks")&&!o.contains("playlist_type")&&!o.contains("url_cover")&&!o.contains("track_count")))return false;
        if(seen.contains(id))return true;seen.insert(id);auto cover=platform==Kugou?kugouCover(o):imageUrl(o.value("url_cover"));
        rows.append(QVariantMap{{"id",id},{"name",name},{"cover",cover},{"provider",platform==Kugou?"kugou":"qishui"},{"kind","playlist"},
            {"ownerId",text(o.value("owner_id"))},{"listId",text(o.value("listid"))},
            {"catalogProtocol",platform==Qishui?(carCollection?"car":"luna"):""},
            {"playlistType",(o.contains("playlist_type")?o.value("playlist_type"):o.value("type")).toVariant()},
            {"count",o.value("count_tracks").toInt(o.value("count").toInt(o.value("songcount").toInt()))}});return true;
    });return rows;
}
QUrl AdditionalMusicBridge::kugouUrl(const QString &host,const QString &path,QMap<QString,QString> query,
        const QString &device,const QJsonObject &credential,qint64 time,bool web,const QByteArray &body,bool audioKey){
    QMap<QString,QString> defaults{{"dfid",credential.value("dfid").toString("-")},{"mid",device},{"uuid","-"},{"appid","1005"},{"clientver","20489"},{"clienttime",QString::number(time)}};
    if(!credential.value("token").toString().isEmpty())defaults["token"]=credential.value("token").toString();
    if(!text(credential.value("userid")).isEmpty())defaults["userid"]=text(credential.value("userid"));
    for(auto it=query.cbegin();it!=query.cend();++it)defaults[it.key()]=it.value();query=defaults;
    if(audioKey)query["key"]=md5((query.value("hash")+"57ae12eb6890223e355ccfcb74edf70d"+query.value("appid")+device+query.value("userid","0")).toUtf8());
    const auto salt=web?QByteArray("NVPh5oo715z5DIWAeQlhMDsWXXQV4hwt"):QByteArray("OIlwieks28dk2k092lksi2UIkp");
    QByteArray params;for(auto it=query.cbegin();it!=query.cend();++it)params+=(it.key()+"="+it.value()).toUtf8();
    query["signature"]=md5(salt+params+body+salt);return urlWithQuery(host,path,query);
}

AdditionalMusicBridge::AdditionalMusicBridge(Platform platform,Library *library,PlaybackFacade *facade,LyricsModel *lyrics,OnlineArtworkCache *artwork,QObject *parent,QNetworkAccessManager *transport)
    :QObject(parent),m_platform(platform),m_library(library),m_facade(facade),m_lyrics(lyrics),m_artwork(artwork),m_pageCache(library,platform==Kugou?"kugou":"qishui"),m_network(this),m_transport(transport?transport:&m_network){
    m_cookies=new MusicCookieJar(m_transport);m_transport->setCookieJar(m_cookies);
    m_poll.setInterval(2500);connect(&m_poll,&QTimer::timeout,this,&AdditionalMusicBridge::pollLogin);
    m_status="连接"+name()+"，发现更多音乐";
    connect(facade,&PlaybackFacade::onlineSourceRequested,this,&AdditionalMusicBridge::requestSource);
}
AdditionalMusicBridge::~AdditionalMusicBridge(){m_poll.stop();for(auto *reply:m_requests.keys()){reply->disconnect(this);reply->abort();}}
QString AdditionalMusicBridge::sessionPath()const{const auto dir=QDir(m_library->root()).filePath("Online");QDir().mkpath(dir);return QDir(dir).filePath(provider()+"-session.bin");}
void AdditionalMusicBridge::activate(){
    if(m_activated)return;m_activated=true;
    m_device=m_library->setting("online."+provider()+".device").toString();
    if(m_device.isEmpty()||(m_platform==Qishui&&!QRegularExpression("^\\d{19}$").match(m_device).hasMatch())){m_device=m_platform==Qishui?QString::number(QRandomGenerator::global()->generate64()%9000000000000000000ULL+1000000000000000000ULL):md5(QUuid::createUuid().toByteArray());m_library->setSetting("online."+provider()+".device",m_device);}
    m_quality=m_library->setting("online."+provider()+".quality","standard").toString();
    QFile file(sessionPath());if(file.open(QIODevice::ReadOnly)){
        const auto state=QJsonDocument::fromJson(MusicCookieJar::protect(file.readAll(),true)).object();
        m_credential=state.value("credential").toObject();m_cookies->restore(QByteArray::fromBase64(state.value("cookies").toString().toLatin1()));
        m_nickname=state.value("nickname").toString();m_avatar=QUrl(state.value("avatar").toString());
        if(!m_credential.isEmpty())refreshAccount();
    }
    emit changed();
}
void AdditionalMusicBridge::saveSession(){
    const QJsonObject state{{"credential",m_credential},{"cookies",QString::fromLatin1(m_cookies->serialize().toBase64())},{"nickname",m_nickname},{"avatar",m_avatar.toString()}};
    const auto protectedBytes=MusicCookieJar::protect(QJsonDocument(state).toJson(QJsonDocument::Compact),false);
    QSaveFile file(sessionPath());if(protectedBytes.isEmpty()||!file.open(QIODevice::WriteOnly)||file.write(protectedBytes)!=protectedBytes.size()||!file.commit())status("已登录，但登录状态未能保存在本机");
}
void AdditionalMusicBridge::setQuality(const QString &quality){if(!QStringList{"standard","exhigh","lossless","hires"}.contains(quality))return;if(m_library->setSetting("online."+provider()+".quality",quality)){m_quality=quality;emit changed();}}
void AdditionalMusicBridge::abort(const QString &purpose){for(auto *reply:m_requests.keys())if(m_requests.value(reply)==purpose)reply->abort();}
void AdditionalMusicBridge::request(const QUrl &url,const QByteArray &body,const QByteArray &type,const QString &purpose,Done done,const QMap<QByteArray,QByteArray> &headers,const QByteArray &responseKey){
    QNetworkRequest request(url);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);
    request.setRawHeader("User-Agent",m_platform==Kugou?"Android15-1070-11083-46-0-DiscoveryDRADProtocol-wifi":url.host()=="beta-luna.douyin.com"?"LunaPC/3.0.0(290101097)":"LunaOpenSDK/1.0.10");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(m_platform==Qishui){
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);
    }
    if(m_platform==Kugou||url.host()=="beta-luna.douyin.com")request.setRawHeader("Referer",m_platform==Kugou?"https://www.kugou.com/":"https://music.douyin.com/");
    if(m_platform==Kugou){request.setRawHeader("mid",m_device.toUtf8());request.setRawHeader("dfid",m_credential.value("dfid").toString("-").toUtf8());request.setRawHeader("clienttime",QUrlQuery(url).queryItemValue("clienttime").toLatin1());request.setRawHeader("kg-rc","1");request.setRawHeader("kg-thash","5d816a0");request.setRawHeader("kg-rec","1");request.setRawHeader("kg-rf","B9EDA08A64250DEFFBCADDEE00F8F25F");}
    if(!type.isEmpty())request.setHeader(QNetworkRequest::ContentTypeHeader,QString::fromLatin1(type));
    for(auto it=headers.cbegin();it!=headers.cend();++it)request.setRawHeader(it.key(),it.value());
    auto *reply=type.isEmpty()?m_transport->get(request):m_transport->post(request,body);m_requests.insert(reply,purpose);emit changed();
    connect(reply,&QNetworkReply::finished,this,[this,reply,purpose,responseKey,done=std::move(done)]{
        m_requests.remove(reply);const auto bytes=reply->isOpen()?reply->readAll():QByteArray();auto object=QJsonDocument::fromJson(bytes).object();
        if(object.isEmpty()&&!responseKey.isEmpty())object=QJsonDocument::fromJson(kugou::aes(bytes,responseKey,false)).object();
        const bool ok=reply->error()==QNetworkReply::NoError&&!object.isEmpty()&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()<300;
        if(m_platform==Qishui&&purpose=="login")object["_relaxer_guard"]=QString::fromLatin1(reply->rawHeader("bd-ticket-guard-server-data"));
        if(!ok){object["_relaxer_http_status"]=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();object["_relaxer_network_error"]=int(reply->error());}
        reply->deleteLater();done(object,ok);emit changed();
    });
}
void AdditionalMusicBridge::registerKugou(const QString &purpose,Done done){
    const auto revision=m_loginRevision;QByteArray secret;const QByteArray alphabet="0123456789abcdefghijklmnopqrstuvwxyz";
    for(int i=0;i<6;++i)secret+=alphabet.at(QRandomGenerator::global()->bounded(int(alphabet.size())));
    // Synthetic client descriptors, never a scan of the user's physical device.
    QJsonObject device{{"availableRamSize",qint64(4983533568LL)},{"availableRomSize",48114719},{"availableSDSize",48114717},
        {"basebandVer",""},{"batteryLevel",100},{"batteryStatus",3},{"brand","Redmi"},{"buildSerial","unknown"},{"device","marble"},
        {"imei",m_device.toUpper()},{"imsi",""},{"manufacturer","Xiaomi"},{"uuid",m_device.toUpper()}};
    for(const auto &name:{"accelerometer","gravity","gyroscope","light","magnetic","orientation","pressure","step_counter","temperature"}){
        device[name]=false;device[QString::fromLatin1(name)+"Value"]="";
    }
    const auto body=kugou::aes(QJsonDocument(device).toJson(QJsonDocument::Compact),secret,true).toBase64();
    const auto portrait=kugou::portrait(QJsonDocument(QJsonObject{{"aes",QString::fromLatin1(secret)},{"uid",text(m_credential.value("userid"))},{"token",m_credential.value("token").toString()}}).toJson(QJsonDocument::Compact));
    if(body.isEmpty()||portrait.isEmpty()){done({},false);return;}
    request(kugouUrl("https://userservice.kugou.com","/risk/v2/r_register_dev",{{"part","1"},{"platid","1"},{"p",QString::fromLatin1(portrait)}},m_device,m_credential,QDateTime::currentSecsSinceEpoch(),false,body),
        body,"application/x-www-form-urlencoded",purpose,[this,revision,done=std::move(done)](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision){done({},false);return;}
            const auto dfid=o.value("data").toObject().value("dfid").toString();
            if(!ok||o.value("status").toInt()!=1||dfid.isEmpty()){done(o,false);return;}
            m_credential["dfid"]=dfid;if(m_loggedIn)saveSession();done(o,true);
        },{},secret);
}
void AdditionalMusicBridge::kg(const QString &host,const QString &path,QMap<QString,QString> query,const QString &purpose,Done done,bool web,const QByteArray &body,bool audioKey,bool post,bool retryDevice){
    if(path=="/everyday_song_recommend"&&(m_credential.value("dfid").toString().isEmpty()||m_credential.value("dfid").toString()=="-")){
        registerKugou(purpose,[this,host,path,query,purpose,done=std::move(done),web,body,audioKey,post,retryDevice](const QJsonObject &o,bool ok){
            if(!ok){done(o,false);return;}kg(host,path,query,purpose,std::move(done),web,body,audioKey,post,retryDevice);
        });return;
    }
    const auto router=QUrl(host).host();
    const bool gateway=router.endsWith(".service.kugou.com")||router=="pubsongscdn.kugou.com";
    const QMap<QByteArray,QByteArray> headers=gateway?QMap<QByteArray,QByteArray>{{"x-router",router.toUtf8()}}:QMap<QByteArray,QByteArray>{};
    request(kugouUrl(gateway?QStringLiteral("https://gateway.kugou.com"):host,path,query,m_device,m_credential,QDateTime::currentSecsSinceEpoch(),web,body,audioKey),body,!post&&body.isEmpty()?QByteArray():QByteArray("application/json"),purpose,
        [this,host,path,query,purpose,done=std::move(done),web,body,audioKey,post,retryDevice](const QJsonObject &o,bool ok){
            const auto code=o.value("errcode").toInt(o.value("error_code").toInt());
            if(retryDevice&&code==20028){
                registerKugou(purpose,[this,host,path,query,purpose,done=std::move(done),web,body,audioKey,post](const QJsonObject &registered,bool success){
                    if(!success){done(registered,false);return;}kg(host,path,query,purpose,std::move(done),web,body,audioKey,post,false);
                });return;
            }
            done(o,ok);
        },headers);
}
void AdditionalMusicBridge::soda(const QString &host,const QString &path,QMap<QString,QString> query,const QString &purpose,Done done,const QByteArray &body,bool form){
    request(urlWithQuery(host,path,query),body,body.isEmpty()?QByteArray():form?QByteArray("application/x-www-form-urlencoded"):QByteArray("application/json"),purpose,std::move(done));
}
void AdditionalMusicBridge::sodaPlatform(const QString &path,QJsonObject body,const QString &purpose,Done done,bool account,int retry){
    const auto revision=m_loginRevision;
    if(account&&!m_loggedIn){done({},false);return;}
    if(!account&&m_sodaClientToken.isEmpty()){
        auto query=qishui::params(m_device);query["trigger_type"]="cold_start";
        soda("https://open.qishui.com",qishui::prefix+"/client/token/get/",query,purpose,
            [this,revision,path,body,purpose,done=std::move(done),retry](const QJsonObject &o,bool ok){
                if(revision!=m_loginRevision){done({},false);return;}
                const auto token=o.value("client_token").toString();
                if(!ok||o.value("err_no").toInt()!=0||token.isEmpty()){done(o,false);return;}
                m_sodaClientToken=token;sodaPlatform(path,body,purpose,std::move(done),false,retry);
            });return;
    }
    auto headers=account?qishui::guardHeaders(m_credential,path):QMap<QByteArray,QByteArray>{{"access-token",m_sodaClientToken.toUtf8()}};
    if(headers.isEmpty()){done({},false);return;}
    body["common_params"]=m_loggedIn?QJsonObject{{"user_id",m_credential.value("userId")}}:QJsonObject{};
    const auto token=m_sodaClientToken;
    request(urlWithQuery("https://open.douyin.com",qishui::prefix+path,qishui::params(m_device)),QJsonDocument(body).toJson(QJsonDocument::Compact),"application/json",purpose,
        [this,revision,path,body,purpose,done=std::move(done),account,retry,token](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision){done({},false);return;}
            auto code=o.value("err_no").toInt();if(!code)code=o.value("data").toObject().value("error_code").toInt();
            if(!account&&code==28001008&&retry==0){if(m_sodaClientToken==token)m_sodaClientToken.clear();sodaPlatform(path,body,purpose,std::move(done),false,1);return;}
            done(o,ok&&code==0);
        },headers);
}
void AdditionalMusicBridge::sodaTicket(const QJsonObject &access,const QJsonObject &guard,quint64 revision,const QString &expectedUser){
    const auto form="access_code="+QUrl::toPercentEncoding(access.value("access_code").toString())+"&open_id="+QUrl::toPercentEncoding(firstText(access,{"open_id","open_uid"}));
    QMap<QByteArray,QByteArray> headers{{"bd-ticket-guard-version","1"}};
    if(guard.contains("csr"))headers["bd-ticket-guard-client-csr"]=guard.value("csr").toString().toUtf8();
    else headers["bd-ticket-guard-client-cert"]=guard.value("guardCert").toString().toUtf8();
    request(urlWithQuery("https://open-client.douyin.com","/passport/open/get_access_ticket/",{{"aid","1128"},{"client_key","aw4bbxfujo741d09"},{"device_platform","android"}}),
        form,"application/x-www-form-urlencoded","login",
        [this,revision,access,guard,expectedUser](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision)return;
            const auto response=QJsonDocument::fromJson(QByteArray::fromBase64(o.value("_relaxer_guard").toString().toLatin1())).object();
            auto cert=response.value("client_cert").toString();
            if(!ok||o.value("err_no").toInt()!=0||o.value("data").toObject().value("error_code").toInt()!=0||response.value("ticket").toString().isEmpty()||response.value("ts_sign").toString().isEmpty()||cert.isEmpty()){
                m_pollPending=false;status("汽水登录票据获取失败，请重新扫码");return;
            }
            if(cert.contains("-----BEGIN"))cert=QString::fromLatin1(cert.toUtf8().toBase64());
            const auto now=QDateTime::currentMSecsSinceEpoch();auto expiry=access.value("expires_time").toVariant().toLongLong();
            expiry=expiry>1000000000000LL?expiry:expiry>1000000000LL?expiry*1000:now+(expiry>0?expiry:21600)*1000;
            if(expiry<=now){m_pollPending=false;status("汽水返回的登录票据已过期，请重新扫码");return;}
            QJsonObject credential{{"deviceId",m_device},{"guardKey",guard.value("guardKey")},{"ticket",response.value("ticket")},
                {"guardTsSign",response.value("ts_sign")},{"guardCert",cert},{"accessCode",access.value("access_code")},
                {"openId",firstText(access,{"open_id","open_uid"})},{"expiresAt",expiry}};
            sodaProfile(credential,revision,expectedUser);
        },headers);
}
void AdditionalMusicBridge::sodaProfile(QJsonObject credential,quint64 revision,const QString &expectedUser){
    const auto path=QStringLiteral("/me/info/");const auto headers=qishui::guardHeaders(credential,path);
    if(headers.isEmpty()){m_pollPending=false;status("汽水授权签名无效，请重新扫码");return;}
    const QJsonObject body{{"common_params",expectedUser.isEmpty()?QJsonObject{}:QJsonObject{{"user_id",expectedUser}}}};
    request(urlWithQuery("https://open.douyin.com",qishui::prefix+path,qishui::params(m_device)),QJsonDocument(body).toJson(QJsonDocument::Compact),"application/json","login",
        [this,revision,credential,expectedUser](const QJsonObject &o,bool ok)mutable{
            if(revision!=m_loginRevision)return;m_pollPending=false;
            const auto profile=o.value("data").toObject().value("my_info").toObject();const auto id=text(profile.value("id"));
            const auto code=o.value("err_no").toInt(o.value("data").toObject().value("error_code").toInt());
            if(!ok||code||id.isEmpty()||(!expectedUser.isEmpty()&&id!=expectedUser)){
                m_loggedIn=false;
                if(code==28001003||code==28001007||code==10007||(!id.isEmpty()&&!expectedUser.isEmpty()&&id!=expectedUser)){m_credential={};QFile::remove(sessionPath());}
                status("汽水账号核验失败，请重新扫码登录");return;
            }
            credential["userId"]=id;m_credential=credential;m_loggedIn=true;m_nickname=profile.value("nickname").toString();
            m_avatar=QUrl(imageUrl(profile.value("medium_avatar_url")));if(m_avatar.isEmpty())m_avatar=QUrl(imageUrl(profile.value("avatar_url")));
            m_membership=accountMembership("qishui",profile);saveSession();status("已登录汽水音乐 · "+m_nickname);
            if(!m_qrKey.isEmpty()){m_qrKey.clear();emit loginCompleted();}
            if(m_kind=="mine")showPage(m_kind,m_argument);
        },headers);
}
void AdditionalMusicBridge::stopLogin(){++m_loginRevision;m_poll.stop();m_qrKey.clear();m_pollPending=false;abort("login");}
void AdditionalMusicBridge::startLogin(){
    activate();stopLogin();const auto revision=m_loginRevision;m_qrImage.clear();status("正在生成"+name()+"登录二维码…");
    if(m_platform==Kugou){
        kg("https://login-user.kugou.com","/v2/qrcode",{{"appid","1001"},{"type","1"},{"plat","4"},{"srcappid","2919"},{"qrcode_txt","https://h5.kugou.com/apps/loginQRCode/html/index.html?appid=1005&"}},"login",[this,revision](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision)return;m_qrKey=firstText(o.value("data").toObject(),{"qrcode","key"});
            if(!ok||m_qrKey.isEmpty()){status("酷狗登录二维码获取失败，请重试");return;}
            m_qrImage=makeQrImage("https://h5.kugou.com/apps/loginQRCode/html/index.html?appid=1005&qrcode="+m_qrKey);status("请用酷狗音乐 App 扫码并确认登录");m_poll.start();
        },true);
    }else{
        if(!QRegularExpression("^\\d{19}$").match(m_device).hasMatch()){
            m_device=QString::number(QRandomGenerator::global()->generate64()%9000000000000000000ULL+1000000000000000000ULL);
            m_library->setSetting("online.qishui.device",m_device);
        }
        soda("https://open.douyin.com","/oauth/get_qrcode/",qishui::qrParams(),"login",[this,revision](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision)return;const auto data=o.value("data").toObject();m_qrKey=data.value("token").toString();
            const auto payload=firstText(data,{"qrcode_index_url","qrcode_url"}),encoded=data.value("qrcode").toString();
            m_qrImage=!payload.isEmpty()?makeQrImage(payload):encoded.startsWith("data:")||encoded.startsWith("https://")?encoded:encoded.isEmpty()?QString():"data:image/png;base64,"+encoded;
            if(!ok||o.value("err_no").toInt()!=0||data.value("error_code").toInt()!=0||m_qrKey.isEmpty()||m_qrImage.isEmpty()){status("汽水登录二维码获取失败，请重试");return;}
            status("请按二维码提示使用抖音 App 扫码并确认汽水音乐登录");m_poll.start();
        });
    }
}
void AdditionalMusicBridge::pollLogin(){
    if(m_pollPending||m_qrKey.isEmpty())return;m_pollPending=true;const auto revision=m_loginRevision;
    if(m_platform==Qishui){
        auto query=qishui::qrParams();query["token"]=m_qrKey;query["timestamp"]=QString::number(QDateTime::currentMSecsSinceEpoch());
        soda("https://open.douyin.com","/oauth/check_qrcode/",query,"login",[this,revision](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision)return;
            const auto data=o.value("data").toObject();const auto state=data.value("status").toString();
            const auto error=firstText(data,{"error_code"});
            if(!ok||o.value("err_no").toInt()!=0||data.value("error_code").toInt()!=0){m_pollPending=false;m_poll.stop();status("汽水扫码验证失败"+(error.isEmpty()?QString():"（错误码 "+error+"）")+"，请刷新二维码");return;}
            if(state=="confirmed"){
                m_poll.stop();const auto code=firstText(data,{"code","auth_code"});
                if(code.isEmpty()){m_pollPending=false;status("汽水未返回扫码授权码，请重新扫码");return;}
                auto params=qishui::params(m_device);params["auth_code"]=code;status("手机已确认，正在完成汽水授权…");
                soda("https://open.qishui.com",qishui::prefix+"/access/code/get/",params,"login",[this,revision](const QJsonObject &access,bool success){
                    if(revision!=m_loginRevision)return;
                    const auto value=access.value("data").isObject()?access.value("data").toObject():access;
                    if(!success||access.value("err_no").toInt()!=0||value.value("access_code").toString().isEmpty()){m_pollPending=false;status("汽水授权码交换失败，请重新扫码");return;}
                    const auto guard=qishui::createGuard();
                    if(guard.isEmpty()){m_pollPending=false;status("无法创建本机汽水授权签名，请重新扫码");return;}
                    sodaTicket(value,guard,revision);
                });return;
            }
            m_pollPending=false;
            if(state=="expired"||state=="refused"){m_poll.stop();status(state=="refused"?"已取消汽水登录，请刷新二维码":"二维码已过期，请刷新二维码");}
            else if(state=="new"||state=="scanned")status(state=="scanned"?"已扫码，请在手机上确认登录":"等待扫码登录汽水音乐");
            else{m_poll.stop();status("汽水返回了未知扫码状态，请刷新二维码");}
        });return;
    }
    auto done=[this,revision](const QJsonObject &o,bool ok){
        if(revision!=m_loginRevision)return;m_pollPending=false;
        if(!ok){m_poll.stop();status(name()+"登录连接失败，请刷新二维码");return;}
        const auto data=o.value("data").isObject()?o.value("data").toObject():o;const auto state=text(data.value("status")).toLower();
        const bool success=state=="4";
        if(success){
            m_poll.stop();
            if(data.value("token").toString().isEmpty()||text(data.value("userid")).isEmpty()){status("酷狗未返回完整登录会话，请重新扫码");return;}
            const auto dfid=m_credential.value("dfid");m_credential=data;if(!dfid.toString().isEmpty())m_credential["dfid"]=dfid;
            refreshAccount();return;
        }
        if(data.value("error_code").toInt()!=0){m_poll.stop();status(name()+"扫码验证失败（错误码 "+text(data.value("error_code"))+"），请刷新二维码");}
        else if(state=="0"||state=="expired"||state=="5"){m_poll.stop();status("二维码已过期，请刷新二维码");}
        else status(state=="2"||state=="scanned"?"已扫码，请在手机上确认登录":"等待扫码登录"+name());
    };
    kg("https://login-user.kugou.com","/v2/get_userinfo_qrcode",{{"plat","4"},{"appid","1005"},{"srcappid","2919"},{"qrcode",m_qrKey}},"login",std::move(done),true);
}
void AdditionalMusicBridge::refreshAccount(){
    const auto revision=m_loginRevision;
    if(m_platform==Kugou){
        m_loggedIn=!m_credential.value("token").toString().isEmpty();m_nickname=firstText(m_credential,{"nickname","username","user_name"});if(m_nickname.isEmpty())m_nickname="酷狗账号 "+text(m_credential.value("userid"));m_avatar=QUrl(firstText(m_credential,{"pic","picurl","avatar","userpic"}));
        m_membership=accountMembership("kugou",m_credential);status(m_loggedIn?"已登录酷狗音乐 · "+m_nickname:"请扫码登录酷狗音乐");
        if(m_loggedIn){saveSession();if(!m_qrKey.isEmpty()){m_qrKey.clear();emit loginCompleted();}if(m_kind=="mine"||m_kind=="daily")showPage(m_kind,m_argument);}return;
    }
    if(m_credential.value("ticket").toString().isEmpty()){status("汽水登录协议已更新，请重新扫码登录");return;}
    m_device=m_credential.value("deviceId").toString();const auto expectedUser=m_credential.value("userId").toString();
    if(m_credential.value("expiresAt").toInteger()<QDateTime::currentMSecsSinceEpoch()+3600000){
        auto query=qishui::params(m_device);query["access_code"]=m_credential.value("accessCode").toString();
        soda("https://open.qishui.com",qishui::prefix+"/access/code/refresh/",query,"login",[this,revision,expectedUser](const QJsonObject &o,bool ok){
            if(revision!=m_loginRevision)return;auto access=o.value("data").isObject()?o.value("data").toObject():o;
            if(!ok||o.value("err_no").toInt()!=0||access.value("access_code").toString().isEmpty()){status("汽水登录续期失败，请重新扫码");return;}
            if(firstText(access,{"open_id","open_uid"}).isEmpty())access["open_id"]=m_credential.value("openId");
            sodaTicket(access,m_credential,revision,expectedUser);
        });return;
    }
    sodaProfile(m_credential,revision,expectedUser);
}
void AdditionalMusicBridge::logout(){
    m_pageCache.clearPrivate();
    stopLogin();++m_listRevision;++m_playRevision;abort("catalog");abort("media");m_credential={};m_cookies->restore({});m_loggedIn=false;m_membership.clear();m_nickname.clear();m_avatar=QUrl();m_qrImage.clear();m_catalogLoading=false;
    m_sodaClientToken.clear();m_playlistCatalog.clear();m_songs.clear();m_playlists.clear();m_currentPlaylist.clear();QFile::remove(sessionPath());status("已退出"+name()+"账号");emit songsChanged();
}
void AdditionalMusicBridge::setSongs(const QJsonValue &response){
    registerSongs(normalizeSongs(m_platform,response));
}
void AdditionalMusicBridge::readSodaPlaylists(quint64 revision,int collection,const QString &cursor,QVariantList rows,QSet<QString> cursors){
    const auto path=collection==0?QStringLiteral("/me/playlist/"):QStringLiteral("/me/collection/mixed/");
    cursors.insert(cursor);
    sodaPlatform(path,{{"count",100},{"cursor",cursor},{"item_types",QJsonArray{"playlist"}}},"catalog",
        [this,revision,collection,rows=std::move(rows),cursors=std::move(cursors)](const QJsonObject &o,bool ok)mutable{
            if(revision!=m_listRevision)return;
            const auto commit=qScopeGuard([this]{if(!m_catalogLoading)cachePage();});
            const auto data=o.value("data").toObject();
            const auto field=collection==0?(data.contains("playlists")?QStringLiteral("playlists"):QStringLiteral("playlist_list")):QStringLiteral("mixed_collections");
            if(!ok||!data.value(field).isArray()){
                m_catalogLoading=false;
                status(collection==0?"汽水自建歌单读取失败，请刷新重试":"汽水收藏歌单读取失败，已读取的歌单仍可使用，请刷新重试");return;
            }
            const auto batch=data.value(field).toArray();QSet<QString> ids;
            for(const auto &row:rows)ids.insert(row.toMap().value("id").toString());
            for(const auto &row:normalizePlaylists(Qishui,batch)){
                const auto id=row.toMap().value("id").toString();if(!ids.contains(id)){rows.append(row);ids.insert(id);}
            }
            m_playlists=rows;for(const auto &row:rows){const auto playlist=row.toMap();m_playlistCatalog[playlist.value("id").toString()]=playlist;}emit changed();
            if(data.value("has_more").toVariant().toBool()){
                const auto next=text(data.value("next_cursor"));
                if(next.isEmpty()||cursors.contains(next)||batch.isEmpty()||cursors.size()>=500){
                    m_catalogLoading=false;status("汽水歌单分页未能继续，已读取的歌单仍可使用，请刷新重试");return;
                }
                readSodaPlaylists(revision,collection,next,std::move(rows),std::move(cursors));return;
            }
            if(collection==0){readSodaPlaylists(revision,1,{},std::move(rows));return;}
            m_catalogLoading=false;status(m_playlists.isEmpty()?"汽水账号暂无自建或收藏歌单":"汽水音乐 · "+QString::number(m_playlists.size())+" 个歌单");
        },true);
}
void AdditionalMusicBridge::requestSodaPlaylist(const QString &id,Done done,bool legacy,bool fallback){
    const auto revision=m_listRevision;
    auto received=[this,id,revision,legacy,fallback,done=std::move(done)](const QJsonObject &response,bool ok)mutable{
        if(revision!=m_listRevision)return;
        auto code=response.value("err_no").toInt();if(!code)code=response.value("data").toObject().value("error_code").toInt();
        // Old pinned records predate the protocol marker. Try the matching
        // public Luna detail once only for this specific contract rejection.
        if(!legacy&&fallback&&code==28001035){requestSodaPlaylist(id,std::move(done),true,false);return;}
        if(ok&&code==0&&response.value("status_code").toInt()==0){
            m_currentPlaylist["catalogProtocol"]=legacy?"luna":"car";
            m_playlistCatalog[id]=m_currentPlaylist;
        }
        done(response,ok);
    };
    const auto type=m_currentPlaylist.value("playlistType");
    if(legacy){
        QJsonObject body{{"playlist_id",id},{"count",50}};
        if(type.isValid()&&!type.isNull())body["playlist_type"]=type.toInt();
        soda("https://beta-luna.douyin.com","/luna/playlist/detail",{},"catalog",std::move(received),QJsonDocument(body).toJson(QJsonDocument::Compact));
    }else sodaPlatform("/playlist/detail/",{{"playlist_id",id},{"owner_id",m_currentPlaylist.value("ownerId").toString()},
        {"count",100},{"cursor","0"},{"type",type.isValid()&&type.toInt()==1?QJsonValue(1):QJsonValue(QJsonValue::Null)}},"catalog",std::move(received));
}
void AdditionalMusicBridge::loadKugouCovers(quint64 revision,const QStringList &ids,int offset){
    if(revision!=m_listRevision||offset>=ids.size())return;
    QJsonArray batch;for(int i=offset;i<qMin(offset+50,int(ids.size()));++i)batch.append(QJsonObject{{"hash",ids[i]},{"audio_id",0}});
    const auto time=QDateTime::currentMSecsSinceEpoch();
    QJsonObject body{{"appid",1005},{"clienttime",time},{"clientver",20489},{"data",batch},
        {"dfid",m_credential.value("dfid").toString("-")},{"mid",m_device},
        {"key",md5(("1005OIlwieks28dk2k092lksi2UIkp20489"+QString::number(time)).toUtf8())}};
    if(!m_credential.value("token").toString().isEmpty())body["token"]=m_credential.value("token");
    if(!text(m_credential.value("userid")).isEmpty())body["userid"]=m_credential.value("userid");
    kg("https://kmr.service.kugou.com","/v1/audio/audio",{},"catalog",
        [this,revision,ids,offset](const QJsonObject &o,bool ok){
            if(revision!=m_listRevision)return;
            if(ok&&text(o.value("status"))!="0"&&o.value("error_code").toInt()==0){
                QHash<QString,QPair<QString,QString>> covers;
                visit(o,[&](const QJsonObject &metadata){
                    const auto hash=kugouHash(metadata).toUpper();const auto cover=kugouCover(metadata);
                    if(hash.isEmpty()||cover.isEmpty())return false;
                    covers[hash]={cover,kugouCover(metadata,240)};return true;
                });
                auto rows=m_songs;bool changed=false;
                for(auto &value:rows){
                    auto row=value.toMap();if(!row.value("cover").toString().isEmpty())continue;
                    const auto found=covers.constFind(row.value("id").toString().toUpper());if(found==covers.cend())continue;
                    row["cover"]=found->first;row["thumbnail"]=found->second;value=row;changed=true;
                    auto track=m_facade->track(row.value("internalId").toLongLong());
                    if(track.id&&track.hash==provider()+":"+row.value("id").toString()){
                        track.onlineMetadata=QJsonObject::fromVariantMap(row);
                        m_artwork->prepare(track,QUrl(found->second),QUrl(found->first),QDir(DiskCache::online(m_library)).filePath(provider()+"-"+QString::number(track.id)+".png"));
                        m_facade->updateOnlineTrack(track);
                    }
                }
                if(changed){registerSongs(std::move(rows));cachePage();if(m_kind=="playlist"&&m_library->setting("online.kugou.pinned").toObject().contains(m_argument))savePinnedPlaylist(m_argument);}
            }
            loadKugouCovers(revision,ids,offset+50);
        },false,QJsonDocument(body).toJson(QJsonDocument::Compact));
}
void AdditionalMusicBridge::registerSongs(QVariantList rows){
    if(m_identifiers.isEmpty()){
        const qint64 base=m_platform==Kugou?3000000000000LL:4000000000000LL;
        for(const auto key:m_facade->queue().items){
            if(key<=base||key>=base+1000000000000LL)continue;
            const auto track=m_facade->track(key);if(!track.hash.startsWith(provider()+":"))continue;
            m_identifiers.insert(track.hash.mid(provider().size()+1),key);m_nextInternal=qMax(m_nextInternal,key-base+1);
        }
    }
    m_songs=std::move(rows);
    for(auto &v:m_songs){auto row=v.toMap();const auto id=row.value("id").toString();auto previous=m_catalog.value(id);qint64 internal=m_identifiers.value(id);if(!internal){internal=(m_platform==Kugou?3000000000000LL:4000000000000LL)+m_nextInternal++;m_identifiers.insert(id,internal);}
        if(m_platform==Kugou&&row.value("cover").toString().isEmpty()&&!previous.value("cover").toString().isEmpty()){
            row["cover"]=previous.value("cover");row["thumbnail"]=previous.value("thumbnail");
        }
        row["internalId"]=QString::number(internal);row["provider"]=provider();row["key"]=provider()+":"+id;m_catalog[id]=row;m_internal[internal]=id;v=row;}
    trimCatalog();emit songsChanged();
}
void AdditionalMusicBridge::trimCatalog(){
    QSet<QString> songs;for(const auto &v:m_songs)songs.insert(v.toMap().value("id").toString());
    trimCatalogMap(m_catalog,songs);
    for(auto it=m_internal.begin();it!=m_internal.end();)if(!m_catalog.contains(it.value()))it=m_internal.erase(it);else ++it;
    QSet<QString> lists;for(const auto &v:m_playlists)lists.insert(v.toMap().value("id").toString());
    lists.insert(m_currentPlaylist.value("id").toString());trimCatalogMap(m_playlistCatalog,lists,64);
}
void AdditionalMusicBridge::cachePage(){
    if(m_catalogLoading||((m_kind=="mine"||m_kind=="daily")&&!m_loggedIn))return;
    m_pageCache.finish({{"songs",m_songs},{"playlists",m_playlists},{"playlist",m_currentPlaylist},{"status",m_status}});
}
void AdditionalMusicBridge::cancelRecommendation(){++m_listRevision;m_pageCache.cancelPending();abort("catalog");m_catalogLoading=false;emit changed();}
void AdditionalMusicBridge::showPage(const QString &kind,const QString &argument,bool refresh){
    activate();
    if(!refresh&&m_catalogLoading&&m_kind==kind&&m_argument==argument)return;
    cancelRecommendation();m_kind=kind;m_argument=argument;
    if((kind=="mine"||kind=="daily")&&!m_loggedIn){readCatalog(kind,argument);return;}
    QVariantMap snapshot;const auto action=m_pageCache.begin(kind+(argument.isEmpty()?QString():":"+argument),refresh,snapshot);
    if(action==OnlinePageCache::Cached){
        m_currentPlaylist=snapshot.value("playlist").toMap();m_playlists=snapshot.value("playlists").toList();
        for(const auto &row:m_playlists){const auto playlist=row.toMap();m_playlistCatalog[playlist.value("id").toString()]=playlist;}
        registerSongs(snapshot.value("songs").toList());status(snapshot.value("status").toString());return;
    }
    readCatalog(kind,argument);
}
void AdditionalMusicBridge::search(const QString &query,int category){showPage(category?"unsupported":"search",query);}
void AdditionalMusicBridge::openPlaylist(const QString &id){showPage("playlist",id);}
void AdditionalMusicBridge::readCatalog(const QString &kind,const QString &argument){
    const auto revision=m_listRevision;m_catalogLoading=true;m_currentPlaylist.clear();m_songs.clear();emit songsChanged();status("正在读取"+name()+"…");
    if(kind=="featured"||kind=="mine")m_playlists.clear();
    if(kind=="playlist"){
        const auto cached=m_library->setting("online."+provider()+".pinned").toObject().value(argument).toObject();
        if(!cached.isEmpty()){m_currentPlaylist=cached.value("playlist").toObject().toVariantMap();registerSongs(cached.value("songs").toArray().toVariantList());}
        const auto playlist=m_playlistCatalog.value(argument);
        if(!playlist.isEmpty()){
            if(m_currentPlaylist.isEmpty())m_currentPlaylist=playlist;
            else for(const auto &key:{"listId","ownerId","catalogProtocol","playlistType"})if(playlist.value(key).isValid()&&!playlist.value(key).toString().isEmpty())m_currentPlaylist[key]=playlist.value(key);
        }
    }
    auto done=[this,revision,kind,argument](const QJsonObject &o,bool ok){
        if(revision!=m_listRevision)return;m_catalogLoading=false;
        const auto commit=qScopeGuard([this]{cachePage();});
        if(kind=="playlist"&&playlistReportedMissing(o)){status(name()+"歌单已不存在");emit playlistMissing(argument);return;}
        const auto code=o.value("status_code").toInt();
        if(!ok||code!=0||o.value("error_code").toInt()!=0||o.value("err_no").toInt()!=0||text(o.value("status"))=="0"){
            QString error;for(const auto &key:{"error_code","status_code","errcode","err_no"}){const auto value=text(o.value(key));if(!value.isEmpty()&&value!="0"){error=value;break;}}
            const auto http=o.value("_relaxer_http_status").toInt();
            if(m_platform==Qishui&&error=="28001035")status("汽水未接受当前歌单详情请求（28001035），请刷新当前页面重试");
            else status(name()+"读取失败"+(http>=400?QString("（HTTP %1），请刷新或重新登录").arg(http):error.isEmpty()?QStringLiteral("，请刷新或重新登录"):QStringLiteral("（平台错误码 ")+error+QStringLiteral("），请刷新或重新登录")));return;
        }
        if(kind=="featured"||kind=="mine"){
            m_playlists=normalizePlaylists(m_platform,o);for(const auto &row:m_playlists){const auto playlist=row.toMap();m_playlistCatalog[playlist.value("id").toString()]=playlist;}status(m_playlists.isEmpty()?"平台暂未返回可用歌单，可尝试搜索歌曲":name()+" · "+QString::number(m_playlists.size())+" 个歌单");emit changed();return;
        }
        if(kind=="playlist"){
            const auto previous=m_currentPlaylist;const auto lists=normalizePlaylists(m_platform,o);
            if(!lists.isEmpty()){
                m_currentPlaylist=lists.first().toMap();m_currentPlaylist["id"]=argument;
                for(const auto &key:{"listId","ownerId","catalogProtocol","playlistType"})if(m_currentPlaylist.value(key).toString().isEmpty())m_currentPlaylist[key]=previous.value(key);
            }
            else if(m_currentPlaylist.isEmpty())m_currentPlaylist=QVariantMap{{"id",argument},{"name",name()+"歌单"},{"cover",QString()},{"provider",provider()}};
        }
        setSongs(o);
        if(kind=="playlist"){m_currentPlaylist["count"]=m_songs.size();m_playlistCatalog[argument]=m_currentPlaylist;emit changed();emit songsChanged();
            const auto pinned=m_library->setting("online."+provider()+".pinned").toObject();if(pinned.contains(argument))savePinnedPlaylist(argument);}
        status(m_songs.isEmpty()?"平台暂未返回歌曲；请检查登录状态或更换关键词":name()+" · "+QString::number(m_songs.size())+" 首歌曲");
        if(m_platform==Kugou){
            QStringList missing;for(const auto &value:m_songs){const auto row=value.toMap();if(row.value("cover").toString().isEmpty())missing.append(row.value("id").toString());}
            loadKugouCovers(revision,missing);
        }
    };
    if((kind=="mine"||kind=="daily")&&!m_loggedIn){m_catalogLoading=false;status("请先登录"+name());return;}
    if(kind=="ai"||kind.startsWith("search-")||kind=="unsupported"){
        m_catalogLoading=false;status("该平台当前接入支持单曲搜索、歌单与播放，暂未提供此分类");return;
    }
    if(m_platform==Kugou){
        if(kind=="search")kg("https://complexsearch.kugou.com","/v2/search/song",{{"keyword",argument},{"page","1"},{"pagesize","50"},{"platform","AndroidFilter"},{"iscorrection","1"},{"privilegefilter","0"},{"area_code","1"},{"dopicfull","1"}},"catalog",std::move(done));
        else if(kind=="playlist"){
            const auto listId=m_currentPlaylist.value("listId").toString();
            if(!listId.isEmpty()&&m_loggedIn){
                const QJsonObject body{{"listid",listId.toLongLong()},{"userid",text(m_credential.value("userid")).toLongLong()},
                    {"area_code",1},{"show_relate_goods",0},{"pagesize",100},{"allplatform",1},{"show_cover",1},{"type",0},{"token",m_credential.value("token")},{"page",1}};
                kg("https://cloudlist.service.kugou.com","/v4/get_list_all_file",{},"catalog",std::move(done),false,QJsonDocument(body).toJson(QJsonDocument::Compact));
            }else kg("https://gateway.kugou.com","/pubsongs/v2/get_other_list_file_nofilt",{{"global_collection_id",argument},{"area_code","1"},{"begin_idx","0"},{"plat","1"},{"type","1"},{"mode","1"},{"personal_switch","1"},{"extend_fields","abtags,hot_cmt,popularization"},{"pagesize","100"}},"catalog",std::move(done));
        }
        else if(kind=="daily")kg("https://everydayrec.service.kugou.com","/everyday_song_recommend",{{"platform","ios"}},"catalog",std::move(done),false,{},false,true);
        else if(kind=="mine"){
            const QJsonObject body{{"userid",text(m_credential.value("userid")).toLongLong()},{"token",m_credential.value("token")},{"total_ver",979},{"type",2},{"page",1},{"pagesize",100}};
            kg("https://cloudlist.service.kugou.com","/v7/get_all_list",{{"plat","1"}},"catalog",std::move(done),false,QJsonDocument(body).toJson(QJsonDocument::Compact));
        }else if(kind=="featured"){
            const auto time=QString::number(QDateTime::currentSecsSinceEpoch());
            const QJsonObject recommend{{"withtag",1},{"withsong",1},{"sort",1},{"ugc",1},{"is_selected",0},{"withrecommend",1},{"area_code",1},{"categoryid",0}};
            const QJsonObject body{{"appid",1005},{"mid",m_device},{"clientver",20489},{"platform","android"},{"clienttime",time.toLongLong()},
                {"userid",m_credential.value("userid").toInteger()},{"module_id",1},{"page",1},{"pagesize",30},
                {"key",md5(("1005OIlwieks28dk2k092lksi2UIkp20489"+time).toUtf8())},{"special_recommend",recommend},{"req_multi",1},{"retrun_min",5},{"return_special_falg",1}};
            kg("https://specialrec.service.kugou.com","/v2/special_recommend",{},"catalog",std::move(done),false,QJsonDocument(body).toJson(QJsonDocument::Compact));
        }else{m_catalogLoading=false;status("酷狗暂未提供此页面");}
    }else{
        if(kind=="search"){
            const auto searchId=QUuid::createUuid().toString(QUuid::WithoutBraces);
            sodaPlatform("/search/",{{"q",argument},{"cursor","0"},{"search_id",searchId},{"from_search_id",searchId},{"search_method","input"},{"search_type","track"},{"search_scene","normal"},{"scene_name","search"},{"ab_param",""},{"last_viewed_media",QJsonObject{}}},"catalog",std::move(done));
        }
        else if(kind=="playlist"){
            const auto protocol=m_currentPlaylist.value("catalogProtocol").toString();
            requestSodaPlaylist(argument,std::move(done),protocol=="luna",protocol.isEmpty());
        }
        else if(kind=="mine")readSodaPlaylists(revision);
        else if(kind=="featured")soda("https://beta-luna.douyin.com","/luna/discover/mix",{},"catalog",std::move(done),"{\"count\":20}");
        else{m_catalogLoading=false;status("汽水每日推荐接口暂不可用，可使用推荐歌单或搜索");}
    }
}
void AdditionalMusicBridge::playSong(const QString &id){QStringList ids;for(const auto &v:m_songs)ids.append(v.toMap().value("id").toString());playSongs(ids,id);}
QVector<Track> AdditionalMusicBridge::tracksFor(const QStringList &ids){
    QVector<Track> tracks;
    for(const auto &id:ids){const auto row=m_catalog.value(id);if(row.isEmpty())continue;Track t;t.onlineMetadata=QJsonObject::fromVariantMap(row);t.id=row.value("internalId").toLongLong();t.title=row.value("title").toString();t.artist=row.value("artist").toString();t.album=row.value("album").toString();t.duration=row.value("duration").toLongLong();t.requiresSubscription=row.value("requiresSubscription").toBool();t.hash=provider()+":"+id;
        m_artwork->prepare(t,QUrl(row.value("thumbnail").toString()),QUrl(row.value("cover").toString()),QDir(DiskCache::online(m_library)).filePath(provider()+"-"+QString::number(t.id)+".png"));tracks.append(t);}
    return tracks;
}
void AdditionalMusicBridge::playSongs(const QStringList &ids,const QString &start){
    const auto tracks=tracksFor(ids);if(tracks.isEmpty())return;
    qint64 first=tracks.first().id;
    for(const auto &track:tracks)if(track.hash.endsWith(":"+start)||track.id==0+start.toLongLong())first=track.id;
    m_facade->playOnline(tracks,first);
}
bool AdditionalMusicBridge::batchSongs(const QStringList &ids,const QString &action){
    const bool ok=m_facade->batchOnline(tracksFor(ids),action);
    if(!ok)status("请先播放在线歌曲，再向在线队列添加所选歌曲；当前播放歌曲不会重复加入");
    return ok;
}

void AdditionalMusicBridge::savePinnedPlaylist(const QString &id){
    QJsonObject records=m_library->setting("online."+provider()+".pinned").toObject();QJsonArray songs;for(const auto &v:m_songs)songs.append(QJsonObject::fromVariantMap(v.toMap()));records[id]=QJsonObject{{"playlist",QJsonObject::fromVariantMap(m_currentPlaylist)},{"songs",songs}};m_library->setSetting("online."+provider()+".pinned",records);
}
void AdditionalMusicBridge::resolveSodaAudio(const QJsonObject &response,const QString &quality,Done done,bool preview){
    const auto source=sodaAudio(response,quality,preview);
    auto finish=[this,response,quality,preview,done=std::move(done)](const QJsonObject &resolved,bool ok)mutable{
        if(!ok&&!preview){
            bool hasPreview=false;visit(response,[&](const QJsonObject &o){
                if(!o.contains("player_info"))return false;
                hasPreview=!o.value("player_info").toObject().value("preview").toObject().isEmpty();return true;
            });
            if(hasPreview){resolveSodaAudio(response,quality,std::move(done),true);return;}
        }
        done(resolved,ok);
    };
    const QUrl info(source.value("playerInfoUrl").toString());
    if(info.isEmpty()){finish(source,!source.value("url").toString().isEmpty());return;}
    if(!sodaMediaUrl(info)){finish({{"failureKind","address"}},false);return;}
    request(info,{}, {},"media",[quality,preview,source,finish=std::move(finish)](const QJsonObject &vod,bool ok)mutable{
        if(!ok){finish(sodaRequestFailure(vod),false);return;}
        auto resolved=sodaAudio(vod,quality,preview);
        if(preview){resolved["previewStartMs"]=source.value("previewStartMs");resolved["previewDurationMs"]=source.value("previewDurationMs");}
        if(resolved.value("url").toString().isEmpty()&&source.value("encrypted").toBool()&&resolved.value("failureKind").toString()=="no-source"){
            resolved["encrypted"]=true;resolved["failureKind"]="protected";
        }
        finish(resolved,!resolved.value("url").toString().isEmpty());
    });
}
void AdditionalMusicBridge::resolveSource(const QVariantMap &song,const QString &quality,Done done){
    if(m_platform==Kugou){
        const auto q=quality=="hires"?"hires":quality=="lossless"?"flac":quality=="exhigh"?"320":"128";
        kg("https://trackercdn.kugou.com","/v5/url",{{"hash",song.value("id").toString().toLower()},{"album_id",song.value("albumId").toString()},{"album_audio_id",song.value("audioId").toString()},{"quality",q},{"area_code","1"},{"ssa_flag","is_fromtrack"},{"version","11430"},{"page_id","151369488"},{"behavior","play"},{"pid","2"},{"cmd","26"},{"pidversion","3001"},{"IsFreePart","0"},{"ppage_id","463467626,350369493,788954147"},{"cdnBackup","1"},{"module",""},{"clientver","11430"}},"media",[done=std::move(done)](const QJsonObject &o,bool ok){
            auto urls=o.value("url").toArray();auto data=o.value("data").toObject();if(urls.isEmpty())urls=data.value("url").toArray();const auto url=urls.isEmpty()?o.value("url").toString():urls.first().toString();done(QJsonObject{{"url",url},{"format",o.value("extName")} },ok&&!url.isEmpty());
        },false,{},true);
    }else{
        sodaPlatform("/media/mget/",{{"media_list",QJsonArray{QJsonObject{{"id",song.value("id").toString()},{"type","track"}}}},{"card_context",QJsonObject{}}},"media",
            [this,quality,done=std::move(done)](const QJsonObject &o,bool ok){
                if(!ok){done(sodaRequestFailure(o),false);return;}
                resolveSodaAudio(o,quality,std::move(done));
            });
    }
}
void AdditionalMusicBridge::requestSource(qint64 internal){
    auto row=m_catalog.value(m_internal.value(internal));if(row.isEmpty()){const auto track=m_facade->track(internal);if(!track.hash.startsWith(provider()+":"))return;row=track.onlineMetadata.toVariantMap();}if(row.isEmpty())return;activate();const auto revision=++m_playRevision;
    const auto fullDuration=row.value("fullDurationMs",row.value("duration")).toLongLong();
    status("正在获取"+name()+"播放资源…");resolveSource(row,m_quality,[this,revision,internal,fullDuration](const QJsonObject &source,bool ok){
        if(revision!=m_playRevision||!m_facade->onlineActive()||m_facade->current().id!=internal)return;
        if(!ok){const auto error=m_platform==Qishui?sodaFailureMessage(source):name()+"未返回当前账号可播放的资源";status(error);m_facade->failOnlineSource(internal,error,m_platform==Qishui&&source.value("failureKind").toString()=="network");return;}
        const QUrl url(source.value("url").toString());if(url.scheme()!="https"&&url.scheme()!="http"){m_facade->failOnlineSource(internal,"平台返回了无效播放地址");return;}
        QList<QUrl> backups;for(const auto &value:source.value("backups").toArray())backups.append(QUrl(value.toString()));
        const QMap<QByteArray,QByteArray> headers=m_platform==Qishui?QMap<QByteArray,QByteArray>{{"User-Agent","LunaOpenSDK/1.0.10"}}:QMap<QByteArray,QByteArray>{};
        const bool preview=m_platform==Qishui&&source.value("preview").toBool();
        if(m_platform==Qishui){
            auto track=m_facade->track(internal);track.onlineMetadata["preview"]=preview;track.onlineMetadata["fullDurationMs"]=fullDuration;
            track.onlineMetadata["previewStartMs"]=source.value("previewStartMs");track.onlineMetadata["previewDurationMs"]=source.value("previewDurationMs");
            track.duration=preview?source.value("previewDurationMs").toInteger():fullDuration;
            m_facade->updateOnlineTrack(track);
        }
        m_facade->setOnlineSource(internal,url,headers,backups);if(!source.value("lyrics").toString().isEmpty())m_lyrics->setOnlineLyrics(internal,source.value("lyrics").toString().toUtf8());status((preview?"正在试听 · ":"正在播放 · ")+name());
    });
}
void AdditionalMusicBridge::resolveDownload(const QVariantMap &song,const QString &quality){
    resolveSource(song,quality,[this,song](const QJsonObject &source,bool ok){
        const QUrl url(source.value("url").toString());
        if(ok&&source.value("preview").toBool()){emit downloadFailed(song,"平台仅提供试听片段，可直接试听，暂不能作为完整歌曲下载");return;}
        if(!ok||(url.scheme()!="https"&&url.scheme()!="http")){emit downloadFailed(song,source.value("encrypted").toBool()?"汽水返回了加密音频，暂不支持下载此资源":name()+"未返回当前账号可下载的资源");return;}
        auto row=song;row["provider"]=provider();row["key"]=provider()+":"+row.value("id").toString();emit downloadReady(row,url,sourceExtension(source));
    });
}
}
