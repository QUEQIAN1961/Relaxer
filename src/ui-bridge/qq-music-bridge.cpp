#include "qq-music-bridge.h"
#include "online-recommendation.h"
#include "online-artwork-cache.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrlQuery>
#include <QUuid>
#include <algorithm>

namespace relaxer {
namespace {
QString idText(const QJsonValue &value){return value.toVariant().toString();}
quint32 hash33(const QByteArray &value,quint32 seed=0){for(auto c:value)seed=seed*33+quint8(c);return seed&0x7fffffff;}
QUrl urlWithQuery(const QString &base,const QJsonObject &params){
    QUrl url(base);QUrlQuery query;for(auto it=params.begin();it!=params.end();++it)query.addQueryItem(it.key(),idText(it.value()));url.setQuery(query);return url;
}
QByteArray formBody(const QJsonObject &params){
    QByteArray body;for(auto it=params.begin();it!=params.end();++it){if(!body.isEmpty())body+='&';body+=QUrl::toPercentEncoding(it.key())+'='+QUrl::toPercentEncoding(idText(it.value()));}return body;
}
QString nicknameIn(const QJsonObject &data){
    for(const auto &key:{"nick","nickname","name"})if(!data.value(key).toString().isEmpty())return data.value(key).toString();
    for(const auto &key:{"info","userInfo","userinfo","user_info"}){const auto child=data.value(key).toObject();for(const auto &name:{"nick","nickname","name"})if(!child.value(name).toString().isEmpty())return child.value(name).toString();}
    return {};
}
QString decodeLyric(const QString &value){return value.startsWith('[')||value.contains('\n')?value:QString::fromUtf8(QByteArray::fromBase64(value.toLatin1()));}
QList<QNetworkCookie> replyCookies(QNetworkReply *reply){
    auto cookies=reply->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie>>();
    // Qt may combine repeated Set-Cookie fields. Split at a cookie name, never
    // at the comma inside an Expires date.
    const QRegularExpression boundary(",(?=\\s*[^\\s;,=]+\\s*=)");
    for(const auto &header:reply->rawHeaderPairs())if(header.first.compare("Set-Cookie",Qt::CaseInsensitive)==0)
        for(const auto &part:QString::fromLatin1(header.second).split(boundary))
            for(const auto &cookie:QNetworkCookie::parseCookies(part.toLatin1()))if(!cookies.contains(cookie))cookies.append(cookie);
    return cookies;
}
}
QqMusicBridge::QqMusicBridge(Library *library,PlaybackFacade *facade,LyricsModel *lyrics,ai::AiRuntime *ai,OnlineArtworkCache *artwork,QObject *parent)
    :QObject(parent),m_library(library),m_facade(facade),m_lyrics(lyrics),m_ai(ai),m_artwork(artwork),m_pageCache(library,"qq"),m_network(this),m_cookies(new MusicCookieJar(&m_network)){
    m_network.setCookieJar(m_cookies);m_poll.setInterval(1800);
    m_pageCommit.setSingleShot(true);connect(this,&QqMusicBridge::changed,this,[this]{if(m_pageCache.pending())m_pageCommit.start(0);});
    connect(&m_pageCommit,&QTimer::timeout,this,[this]{
        if(!m_pageCache.pending()||!m_requests.isEmpty()||m_aiTask)return;
        const auto before=m_pageCache.before();const bool metadataChanged=m_pageCache.quiet()&&before.value("songs").toList()==m_songs&&before.value("playlist").toMap()!=m_currentPlaylist;
        m_pageCache.finish({{"songs",m_songs},{"playlists",m_playlists},{"searchResults",m_searchResults},{"playlist",m_currentPlaylist},{"status",m_status}});if(metadataChanged)emit songsChanged();emit changed();
    });
    const auto restoreQuality=[this]{auto saved=m_library->setting("online.quality","hires").toString();if(!QStringList{"standard","exhigh","lossless","hires"}.contains(saved))saved="hires";if(m_quality!=saved){m_quality=saved;emit changed();}};
    restoreQuality();connect(library,&Library::settingsUpdated,this,[restoreQuality](const QStringList &keys){if(keys.contains("online.quality"))restoreQuality();});
    connect(&m_poll,&QTimer::timeout,this,&QqMusicBridge::pollLogin);
    connect(facade,&PlaybackFacade::onlineSourceRequested,this,&QqMusicBridge::requestSource);
    connect(m_ai->requests(),&ai::AiRequestCoordinator::completionFinished,this,[this](quint64 task,const QString &,const ai::ChatCompletionResult &result){
        if(task!=m_aiTask)return;m_aiTask=0;emit changed();if(m_aiRevision!=m_listRevision)return;
        if(!result.ok()){setStatus("AI 推荐失败："+result.error.message);return;}
        auto text=result.content.trimmed();if(text.startsWith("```")){text=text.mid(text.indexOf('\n')+1);text=text.left(text.lastIndexOf("```"));}
        const auto selected=QJsonDocument::fromJson(text.toUtf8()).object().value("recommendations").toArray();
        QVariantList ranked;QSet<QString> used;
        for(const auto &value:selected){const auto object=value.toObject();const auto id=idText(object.value("id"));if(used.contains(id))continue;
            for(const auto &candidate:m_songs){auto row=candidate.toMap();if(row.value("id").toString()!=id)continue;row["reason"]=object.value("reason").toString();ranked.append(row);used.insert(id);break;}
            if(ranked.size()>=10)break;
        }
        if(ranked.isEmpty()){setStatus("AI 未返回有效候选，请重试");return;}
        m_songs=ranked;emit songsChanged();QSaveFile cache(onlinePath("qq-recommendations.json"));
        if(cache.open(QIODevice::WriteOnly)){cache.write(QJsonDocument(QJsonArray::fromVariantList(m_songs)).toJson(QJsonDocument::Compact));cache.commit();}
        setStatus(QString("AI 已结合本地收听偏好推荐 %1 首 QQ 音乐歌曲").arg(m_songs.size()));
    });
}
QqMusicBridge::~QqMusicBridge(){m_poll.stop();cancelRecommendation();for(auto *reply:m_requests){reply->disconnect(this);reply->abort();}}
QString QqMusicBridge::onlinePath(const QString &file) const {const auto folder=QDir(m_library->root()).filePath("Online");QDir().mkpath(folder);return QDir(folder).filePath(file);}
QString QqMusicBridge::userId() const {const auto text=m_credential.value("str_musicid").toString();return text.isEmpty()?idText(m_credential.value("musicid")):text;}
void QqMusicBridge::setStatus(const QString &status){if(!m_pageCache.quiet())m_status=status;emit changed();}
bool QqMusicBridge::beginPage(const QString &key){
    QVariantMap snapshot;const bool force=m_forcePage;m_forcePage=false;const auto action=m_pageCache.begin(key,force,snapshot);
    if(!snapshot.isEmpty()){
        const bool changedSongs=m_songs!=snapshot.value("songs").toList()||m_currentPlaylist!=snapshot.value("playlist").toMap();
        m_songs=snapshot.value("songs").toList();m_playlists=snapshot.value("playlists").toList();m_searchResults=snapshot.value("searchResults").toList();m_currentPlaylist=snapshot.value("playlist").toMap();m_status=snapshot.value("status").toString();
        for(const auto &v:m_songs){const auto row=v.toMap();m_catalog.insert(row.value("id").toLongLong(),row);}if(changedSongs||key.startsWith("playlist:"))emit songsChanged();emit this->changed();
    }
    if(action==OnlinePageCache::Cached){++m_listRevision;return true;}return action==OnlinePageCache::Pending;
}
void QqMusicBridge::showPage(const QString &kind,const QString &argument,bool refresh){
    m_forcePage=refresh;
    if(kind=="featured")recommendedPlaylists();else if(kind=="mine")loadPlaylists();else if(kind=="daily")dailyRecommendations();else if(kind=="ai")recommend();else if(kind=="search")search(argument);else if(kind=="search-artists")search(argument,1);else if(kind=="search-albums")search(argument,2);else if(kind=="search-playlists")search(argument,3);else if(kind=="artist"||kind=="album")openCollection(kind,argument);else if(kind=="playlist")openPlaylist(argument);m_forcePage=false;
}
void QqMusicBridge::trace(const QString &message) const {
    if(!qEnvironmentVariableIsSet("RELAXER_QQ_LOGIN_TRACE"))return;
    QFile file(onlinePath("qq-login-trace.txt"));if(file.open(QIODevice::WriteOnly|QIODevice::Append))file.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+" "+message+"\n").toUtf8());
}
QNetworkReply *QqMusicBridge::request(const QUrl &url,const QByteArray &body,const QByteArray &contentType,ReplyDone done,bool login,int timeout){
    QNetworkRequest req(url);req.setTransferTimeout(timeout);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("User-Agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    req.setRawHeader("Referer",url.host().contains("weixin.qq.com")?"https://open.weixin.qq.com/":url.host().contains("ptlogin")||url.host()=="graph.qq.com"?"https://xui.ptlogin2.qq.com/":"https://y.qq.com/");
    if(!contentType.isEmpty())req.setRawHeader("Content-Type",contentType);
    auto *reply=contentType.isEmpty()?m_network.get(req):m_network.post(req,body);m_requests.insert(reply);if(login)m_loginRequest=reply;emit changed();
    connect(reply,&QNetworkReply::finished,this,[this,reply,done=std::move(done),login]{
        m_requests.remove(reply);if(m_loginRequest==reply)m_loginRequest.clear();const auto bytes=reply->isOpen()?reply->readAll():QByteArray();
        if(login){
            const auto cookies=replyCookies(reply);m_cookies->setCookiesFromUrl(cookies,reply->url());
            QStringList names;for(const auto &cookie:cookies)if(!names.contains(QString::fromLatin1(cookie.name())))names.append(QString::fromLatin1(cookie.name()));
            const auto location=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            trace(QString("%1%2 http=%3 network=%4 cookies=%5 redirect=%6%7").arg(reply->url().host(),reply->url().path()).arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(int(reply->error())).arg(names.join(','),location.host(),location.path()));
        }
        emit changed();done(reply,bytes);reply->deleteLater();
    });return reply;
}
void QqMusicBridge::cgi(const QString &module,const QString &method,const QJsonObject &param,JsonDone done,const QJsonObject &extraComm,bool searchEnvelope,bool login){
    const auto key=searchEnvelope?module:QString("req_0");QJsonObject envelope{{key,QJsonObject{{"module",module},{"method",method},{"param",param}}}};
    if(!searchEnvelope){
        const auto musicKey=m_credential.value("musickey").toString();const auto token=hash33(musicKey.toUtf8(),5381);
        QJsonObject comm{{"ct",24},{"cv",4747474},{"platform","yqq.json"},{"uin",userId().isEmpty()?"0":userId()},
            {"g_tk",int(token)},{"g_tk_new_20200303",int(token)},{"format","json"},{"inCharset","utf-8"},{"outCharset","utf-8"},{"notice",0},{"need_new_code",1}};
        if(!musicKey.isEmpty()){comm.insert("authst",musicKey);comm.insert("tmeLoginType",m_credential.value("loginType"));}
        for(auto it=extraComm.begin();it!=extraComm.end();++it)comm.insert(it.key(),it.value());envelope.insert("comm",comm);
    }
    request(QUrl("https://u.y.qq.com/cgi-bin/musicu.fcg"),QJsonDocument(envelope).toJson(QJsonDocument::Compact),"application/json",
        [this,key,module,method,done=std::move(done),login](QNetworkReply *reply,const QByteArray &bytes){
            const auto root=QJsonDocument::fromJson(bytes).object();auto result=root.value(key).toObject();
            if(reply->error()!=QNetworkReply::NoError||root.isEmpty())result={{"code",-1}};
            else if(root.value("code").toInt()!=0)result={{"code",root.value("code")}};
            else if(result.isEmpty())result={{"code",-2}};
            if(login)trace(QString("%1/%2 code=%3").arg(module,method).arg(result.value("code").toInt()));
            done(result);
        },login);
}
void QqMusicBridge::installMusicCookies(){
    const auto id=userId().toLatin1(),key=m_credential.value("musickey").toString().toLatin1();
    QList<QNetworkCookie> cookies;for(const auto &pair:{qMakePair(QByteArray("uin"),id),qMakePair(QByteArray("qqmusic_uin"),id),qMakePair(QByteArray("qm_keyst"),key),qMakePair(QByteArray("qqmusic_key"),key),qMakePair(QByteArray("tmeLoginType"),QByteArray::number(m_credential.value("loginType").toInt()))}){
        QNetworkCookie cookie(pair.first,pair.second);cookie.setDomain(".y.qq.com");cookie.setPath("/");cookie.setSecure(true);cookies.append(cookie);
    }m_cookies->setCookiesFromUrl(cookies,QUrl("https://y.qq.com/"));
}
void QqMusicBridge::saveSession(){
    const QJsonObject value{{"credential",m_credential},{"cookies",QString::fromLatin1(m_cookies->serialize().toBase64())}};
    const auto bytes=MusicCookieJar::protect(QJsonDocument(value).toJson(QJsonDocument::Compact),false);
    QSaveFile file(onlinePath("qq-session.bin"));if(bytes.isEmpty()||!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())setStatus("QQ 音乐已登录，本次会话未能保存在本机");
}
void QqMusicBridge::activate(){
    if(m_activated)return;m_activated=true;QFile file(onlinePath("qq-session.bin"));
    if(file.open(QIODevice::ReadOnly)){
        const auto object=QJsonDocument::fromJson(MusicCookieJar::protect(file.readAll(),true)).object();m_credential=object.value("credential").toObject();
        m_cookies->restore(QByteArray::fromBase64(object.value("cookies").toString().toLatin1()));
        if(!m_credential.value("musickey").toString().isEmpty()&&userId().toLongLong()>0){m_loginChannel=m_credential.value("loginType").toInt()==1?"wx":"qq";installMusicCookies();refreshAccount();}
    }
    QFile cache(onlinePath("qq-recommendations.json"));if(cache.open(QIODevice::ReadOnly)){m_songs=QJsonDocument::fromJson(cache.readAll()).array().toVariantList();for(const auto &v:m_songs){const auto row=v.toMap();m_catalog.insert(row.value("id").toLongLong(),row);}emit songsChanged();}
}
void QqMusicBridge::setLoginChannel(const QString &channel){if(channel==m_loginChannel||!QStringList{"qq","wx"}.contains(channel))return;stopLogin();m_loginChannel=channel;m_qrImage.clear();emit changed();}
void QqMusicBridge::setQuality(const QString &quality){if(!QStringList{"standard","exhigh","lossless","hires"}.contains(quality)||quality==m_quality)return;if(!m_library->setSetting("online.quality",quality))setStatus("音质设置未能保存，请重试");}
void QqMusicBridge::stopLogin(){++m_loginRevision;m_poll.stop();m_qrKey.clear();if(m_loginRequest){auto reply=m_loginRequest;m_loginRequest.clear();reply->abort();}}
void QqMusicBridge::startLogin(){
    stopLogin();const auto revision=m_loginRevision;m_qrImage.clear();setStatus("正在生成登录二维码…");
    if(!m_loggedIn){m_credential={};m_cookies->restore({});}
    if(m_loginChannel=="wx"){
        const auto url=urlWithQuery("https://open.weixin.qq.com/connect/qrconnect",{{"appid","wx48db31d50e334801"},{"redirect_uri","https://y.qq.com/portal/wx_redirect.html?login_type=2&surl=https://y.qq.com/"},{"response_type","code"},{"scope","snsapi_login"},{"state","STATE"}});
        request(url,{}, {},[this,revision](QNetworkReply *,const QByteArray &bytes){
            if(revision!=m_loginRevision)return;
            m_qrKey=QRegularExpression("/connect/qrcode/([\\w-]+)").match(QString::fromUtf8(bytes)).captured(1);
            if(m_qrKey.isEmpty()){setStatus("微信登录二维码生成失败，请重试");return;}
            request(QUrl("https://open.weixin.qq.com/connect/qrcode/"+m_qrKey),{}, {},[this,revision](QNetworkReply *,const QByteArray &image){
                if(revision!=m_loginRevision)return;if(QImage::fromData(image).isNull()){setStatus("微信二维码读取失败，请重试");return;}
                m_qrImage="data:image/jpeg;base64,"+QString::fromLatin1(image.toBase64());setStatus("请用微信扫码，并在手机上确认登录 QQ 音乐");m_poll.start();
            },true);
        },true);
    }else{
        const auto url=urlWithQuery("https://ssl.ptlogin2.qq.com/ptqrshow",{{"appid","716027609"},{"e","2"},{"l","M"},{"s","3"},{"d","72"},{"v","4"},{"t",QString::number(QRandomGenerator::global()->generateDouble(),'f',16)},{"daid","383"},{"pt_3rd_aid","100497308"}});
        request(url,{}, {},[this,revision](QNetworkReply *reply,const QByteArray &bytes){
            if(revision!=m_loginRevision)return;
            for(const auto &cookie:replyCookies(reply))if(cookie.name()=="qrsig")m_qrKey=QString::fromLatin1(cookie.value());
            if(m_qrKey.isEmpty()||QImage::fromData(bytes).isNull()){setStatus("QQ 登录二维码生成失败，请重试");return;}
            m_qrImage="data:image/png;base64,"+QString::fromLatin1(bytes.toBase64());setStatus("请用手机 QQ 扫码，并在手机上确认登录 QQ 音乐");m_poll.start();
        },true);
    }
}
void QqMusicBridge::pollLogin(){
    if(m_qrKey.isEmpty()||m_loginRequest)return;const auto revision=m_loginRevision;const bool wechat=m_loginChannel=="wx";
    QUrl url=wechat?urlWithQuery("https://lp.open.weixin.qq.com/connect/l/qrconnect",{{"uuid",m_qrKey},{"_",QString::number(QDateTime::currentMSecsSinceEpoch())}})
        :urlWithQuery("https://ssl.ptlogin2.qq.com/ptqrlogin",{{"u1","https://graph.qq.com/oauth2.0/login_jump"},{"ptqrtoken",QString::number(hash33(m_qrKey.toLatin1()))},
            {"ptredirect","0"},{"h","1"},{"t","1"},{"g","1"},{"from_ui","1"},{"ptlang","2052"},{"action","0-0-"+QString::number(QDateTime::currentMSecsSinceEpoch())},
            {"js_ver","20102616"},{"js_type","1"},{"pt_uistyle","40"},{"aid","716027609"},{"daid","383"},{"pt_3rd_aid","100497308"},{"has_onekey","1"}});
    request(url,{}, {},[this,revision,wechat](QNetworkReply *reply,const QByteArray &bytes){
        if(revision!=m_loginRevision)return;
        if(wechat&&reply->error()==QNetworkReply::TimeoutError)return;
        if(reply->error()!=QNetworkReply::NoError){m_poll.stop();setStatus("QQ 音乐登录连接失败，请刷新二维码重试");return;}
        const auto text=QString::fromUtf8(bytes);QStringList args;int code=-1;
        if(wechat){const auto match=QRegularExpression("wx_errcode\\s*=\\s*(\\d+)").match(text);if(match.hasMatch())code=match.captured(1).toInt();}
        else{auto matches=QRegularExpression("'((?:\\\\.|[^'])*)'").globalMatch(text);while(matches.hasNext())args.append(matches.next().captured(1));if(!args.isEmpty())code=args.first().toInt();}
        trace(QString("poll channel=%1 code=%2").arg(wechat?"wx":"qq").arg(code));
        if(code==66||code==408)return;
        if(code==67||code==404){setStatus("已扫码，请在手机上确认登录 QQ 音乐");return;}
        m_poll.stop();
        if(code==0&&args.size()>=3){setStatus("已确认，正在连接 QQ 音乐账号…");authorizeQq(QUrl(args[2]),revision);}
        else if(code==405){const auto auth=QRegularExpression("wx_code\\s*=\\s*['\"]([^'\"]+)['\"]").match(text).captured(1);if(auth.isEmpty())setStatus("微信授权结果缺失，请重新扫码");else{setStatus("已确认，正在连接 QQ 音乐账号…");exchangeLogin(auth,revision);}}
        else if(code==65||code==402)setStatus("二维码已过期，请刷新二维码");
        else if(code==68||code==403)setStatus("本次登录已取消，可刷新二维码重试");
        else setStatus(QString("QQ 音乐扫码登录未完成（%1），请刷新二维码重试").arg(code));
    },true,wechat?35000:15000);
}
void QqMusicBridge::authorizeQq(const QUrl &callback,quint64 revision){
    const QUrlQuery input(callback);const auto uin=input.queryItemValue("uin"),sig=input.queryItemValue("ptsigx");
    if(uin.isEmpty()||sig.isEmpty()){setStatus("QQ 授权结果缺失，请重新扫码");return;}
    const auto url=urlWithQuery("https://ssl.ptlogin2.graph.qq.com/check_sig",{{"uin",uin},{"pttype","1"},{"service","ptqrlogin"},{"nodirect","0"},{"ptsigx",sig},
        {"s_url","https://graph.qq.com/oauth2.0/login_jump"},{"ptlang","2052"},{"ptredirect","100"},{"aid","716027609"},{"daid","383"},{"j_later","0"},
        {"low_login_hour","0"},{"regmaster","0"},{"pt_login_type","3"},{"pt_aid","0"},{"pt_aaid","16"},{"pt_light","0"},{"pt_3rd_aid","100497308"}});
    continueQqAuthorization(url,revision,4);
}
void QqMusicBridge::continueQqAuthorization(const QUrl &url,quint64 revision,int redirectsLeft){
    request(url,{}, {},[this,revision,redirectsLeft](QNetworkReply *reply,const QByteArray &){
        if(revision!=m_loginRevision)return;QByteArray pSkey;
        const auto cookies=replyCookies(reply);
        for(const auto &cookie:cookies)if(cookie.name()=="p_skey"||cookie.name()=="p-skey"||cookie.name()=="pskey")pSkey=cookie.value();
        if(pSkey.isEmpty())for(const auto &cookie:m_cookies->cookiesForUrl(QUrl("https://graph.qq.com/oauth2.0/authorize")))if(cookie.name()=="p_skey")pSkey=cookie.value();
        const auto target=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();const auto next=reply->url().resolved(target);
        if(pSkey.isEmpty()&&!target.isEmpty()&&redirectsLeft>0&&next.scheme()=="https"&&(next.host()=="qq.com"||next.host().endsWith(".qq.com"))){continueQqAuthorization(next,revision,redirectsLeft-1);return;}
        if(pSkey.isEmpty()){setStatus("QQ 授权票据读取失败，请重新扫码");return;}
        const auto form=formBody({{"response_type","code"},{"client_id","100497308"},{"redirect_uri","https://y.qq.com/portal/wx_redirect.html?login_type=1&surl=https://y.qq.com/"},
            {"scope","get_user_info,get_app_friends"},{"state","state"},{"switch",""},{"from_ptlogin","1"},{"src","1"},{"update_auth","1"},{"openapi","1010_1030"},
            {"g_tk",QString::number(hash33(pSkey,5381))},{"auth_time",QString::number(QDateTime::currentMSecsSinceEpoch())},{"ui",QUuid::createUuid().toString(QUuid::WithoutBraces)}});
        request(QUrl("https://graph.qq.com/oauth2.0/authorize"),form,"application/x-www-form-urlencoded",[this,revision](QNetworkReply *authReply,const QByteArray &){
            if(revision!=m_loginRevision)return;const QUrl location=authReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();const auto code=QUrlQuery(location).queryItemValue("code");
            if(code.isEmpty()){setStatus("QQ 音乐账号授权未完成，请重新扫码");return;}exchangeLogin(code,revision);
        },true);
    },true);
}
void QqMusicBridge::exchangeLogin(const QString &code,quint64 revision){
    const bool wx=m_loginChannel=="wx";QJsonObject param{{"code",code}};if(wx)param.insert("strAppid","wx48db31d50e334801");
    cgi(wx?"music.login.LoginServer":"QQConnectLogin.LoginServer",wx?"Login":"QQLogin",param,[this,revision](const QJsonObject &result){
        if(revision!=m_loginRevision)return;if(result.value("code").toInt()!=0){setStatus(QString("QQ 音乐账号登录未完成（%1），请刷新二维码重试").arg(result.value("code").toInt()));return;}acceptCredential(result.value("data").toObject(),revision);
    },{{"tmeLoginType",wx?1:2}},false,true);
}
void QqMusicBridge::acceptCredential(const QJsonObject &data,quint64 revision){
    if(revision!=m_loginRevision)return;
    const auto key=data.value("musickey").toString();const auto id=idText(data.value("musicid"));
    if(key.isEmpty()||id.toLongLong()<=0){setStatus("QQ 音乐未返回完整登录会话，请重新扫码");return;}
    m_credential=data;if(!m_credential.contains("loginType"))m_credential.insert("loginType",m_loginChannel=="wx"?1:2);
    installMusicCookies();saveSession();refreshAccount();
}
void QqMusicBridge::refreshAccount(){
    const auto credential=m_credential;
    cgi("music.UserInfo.userInfoServer","GetLoginUserInfo",{},[this,credential](const QJsonObject &result){
        if(credential!=m_credential)return;const int code=result.value("code").toInt();
        if(code!=0){m_loggedIn=false;m_nickname.clear();m_avatarUrl.clear();setStatus(QString("QQ 音乐账号读取失败（%1），请重新扫码登录").arg(code));return;}
        m_loggedIn=true;m_nickname=nicknameIn(result.value("data").toObject());if(m_nickname.isEmpty())m_nickname=nicknameIn(m_credential);if(m_nickname.isEmpty())m_nickname="QQ 音乐账号";
        m_avatarUrl=QUrl(result.value("data").toObject().value("info").toObject().value("logo").toString());
        setStatus("已登录 QQ 音乐 · "+m_nickname);
        cgi("VipLogin.VipLoginInter","vip_login_base",{},[this,credential](const QJsonObject &vip){
            if(credential!=m_credential||!m_loggedIn||vip.value("code").toInt()!=0)return;const auto data=vip.value("data").toObject(),identity=data.value("identity").toObject();
            m_membership=identity.value("svip").toInt()||data.value("svip").toInt()?"超级会员":identity.value("vip").toInt()?"绿钻会员":"普通账号";emit changed();
        });
    },{},false,true);
}
void QqMusicBridge::logout(){
    m_pageCache.clearPrivate();
    stopLogin();++m_playRevision;++m_listRevision;cancelRecommendation();m_credential={};m_cookies->restore({});QFile::remove(onlinePath("qq-session.bin"));
    m_loggedIn=false;m_nickname.clear();m_avatarUrl.clear();m_membership.clear();m_qrImage.clear();m_playlists.clear();
    if(m_facade->onlineActive()&&m_facade->current().hash.startsWith("qqmusic:"))m_facade->stopOnline();setStatus("已退出 QQ 音乐账号");
}
QVariantMap QqMusicBridge::song(const QJsonObject &object) const {
    const auto value=object.contains("track")?object.value("track").toObject():object;const auto album=value.value("album").toObject();QStringList artists;
    for(const auto &artist:value.value("singer").toArray())artists.append(artist.toObject().value("name").toString());const auto albumMid=album.value("mid").toString();
    return {{"id",idText(value.value("id"))},{"mid",value.value("mid").toString()},{"mediaMid",value.value("file").toObject().value("media_mid").toString()},
        {"songType",value.value("type").toInt()},{"requiresSubscription",value.value("pay").toObject().value("pay_play").toInt()!=0},
        {"title",value.value("name").toString()},{"artist",artists.join(" / ")},{"album",album.value("name").toString()},
        {"cover",albumMid.isEmpty()?QString():"https://y.gtimg.cn/music/photo_new/T002M000"+albumMid+".jpg"},
        {"thumbnail",albumMid.isEmpty()?QString():"https://y.gtimg.cn/music/photo_new/T002R300x300M000"+albumMid+".jpg"},
        {"duration",value.value("interval").toVariant().toLongLong()*1000},{"reason",QString()}};
}
void QqMusicBridge::setSongs(const QJsonArray &songs){
    QVariantList next;QSet<qint64> ids;for(const auto &value:songs){const auto row=song(value.toObject());const auto id=row.value("id").toLongLong();if(id<=0||ids.contains(id)||row.value("mid").toString().isEmpty())continue;ids.insert(id);m_catalog.insert(id,row);next.append(row);}if(next==m_songs)return;m_songs=next;emit songsChanged();
}
void QqMusicBridge::search(const QString &query,int category){
    if(category<0||category>3||query.trimmed().isEmpty()||beginPage("search:"+QString::number(category)+":"+query.trimmed()))return;
    m_currentPlaylist.clear();cancelRecommendation();const auto revision=++m_listRevision;
    m_searchResults.clear();setStatus("正在搜索 QQ 音乐…");
    const auto rows=[category](const QJsonObject &result){
        const QString keys[]{"song","singer","album","songlist"};
        const auto body=result.value("data").toObject().value("body").toObject();
        const auto section=body.contains(keys[category])?body.value(keys[category]):body.value("item_"+keys[category]);
        return section.isArray()?section.toArray():section.toObject().value("list").toArray();
    };
    const auto publish=[this,revision,category](const QJsonArray &items){
        if(revision!=m_listRevision)return;
        const QString kinds[]{"","artist","album","playlist"};
        if(category==0){setSongs(items);setStatus(QString("QQ 音乐 · 找到 %1 首歌曲").arg(m_songs.size()));return;}
        for(const auto &value:items){
            const auto item=value.toObject();QString id,name,cover;int count=0;
            if(category==1){id=item.value("singerMid").toString(item.value("singerMID").toString(item.value("mid").toString()));name=item.value("singerName").toString(item.value("singername").toString(item.value("name").toString()));cover=item.value("singerPic").toString("https://y.gtimg.cn/music/photo_new/T001R300x300M000"+id+".jpg");count=item.value("songNum").toInt();}
            else if(category==2){id=item.value("albumMID").toString(item.value("albumMid").toString(item.value("mid").toString()));name=item.value("albumName").toString(item.value("albumname").toString(item.value("name").toString()));cover=item.value("albumPic").toString(item.value("pic").toString("https://y.gtimg.cn/music/photo_new/T002R300x300M000"+id+".jpg"));count=item.value("song_count").toInt();}
            else{id=idText(item.value("dissid"));name=item.value("dissname").toString();cover=item.value("imgurl").toString(item.value("picurl").toString());count=item.value("song_count").toInt();}
            if(id.isEmpty())continue;
            const QVariantMap row{{"id",id},{"name",name},{"cover",cover},{"count",count},{"provider","qq"},{"kind",kinds[category]}};
            m_searchResults.append(row);m_collections.insert(kinds[category]+":"+id,row);
        }
        setStatus(QString("找到 %1 个%2").arg(m_searchResults.size()).arg(category==1?"歌手":category==2?"专辑":"歌单"));
    };
    // The current desktop search endpoint rejects a page size of 20.
    // Fetch two supported pages and publish the combined result once.
    QJsonObject param{{"search_type",category},{"query",query.trimmed()},{"page_num",1},{"num_per_page",10},{"highlight",false},
        {"searchid",QString::number(QRandomGenerator::global()->generate64()>>1)},{"grp",true}};
    cgi("music.search.SearchCgiService","DoSearchForQQMusicDesktop",param,[this,revision,param,rows,publish](const QJsonObject &result)mutable{
        if(revision!=m_listRevision)return;
        if(result.value("code").toInt()!=0){setStatus(QString("QQ 音乐搜索失败（%1），请稍后重试").arg(result.value("code").toInt()));return;}
        const auto first=rows(result);if(first.size()<10){publish(first);return;}
        param["page_num"]=2;
        cgi("music.search.SearchCgiService","DoSearchForQQMusicDesktop",param,[this,revision,first,rows,publish](const QJsonObject &next){
            if(revision!=m_listRevision)return;
            auto all=first;if(next.value("code").toInt()==0)for(const auto &row:rows(next))all.append(row);publish(all);
            if(next.value("code").toInt()!=0)setStatus(m_status+" · 后续结果读取失败，可刷新重试");
        },{},true);
    },{},true);
}
void QqMusicBridge::openCollection(const QString &kind,const QString &id){
    if(beginPage(kind+":"+id))return;
    cancelRecommendation();const auto revision=++m_listRevision;
    m_currentPlaylist=m_collections.value(kind+":"+id);m_currentPlaylist["id"]=id;m_currentPlaylist["kind"]=kind;
    setSongs({});setStatus(kind=="artist"?"正在读取歌手歌曲…":"正在读取专辑…");
    const auto module=kind=="artist"?QString("musichall.song_list_server"):QString("music.musichallAlbum.AlbumSongList");
    const auto method=kind=="artist"?QString("GetSingerSongList"):QString("GetAlbumSongList");
    QJsonObject param=kind=="artist"?QJsonObject{{"singerMid",id},{"order",1},{"number",100},{"begin",0}}:QJsonObject{{"albumMid",id},{"num",300},{"begin",0}};
    cgi(module,method,param,[this,revision,kind](const QJsonObject &result){
        if(revision!=m_listRevision)return;
        if(result.value("code").toInt()!=0){setStatus("歌曲读取失败，请重试");return;}
        const auto data=result.value("data").toObject();QJsonArray tracks;
        const auto items=data.value("songList").toArray();
        for(const auto &value:items){const auto row=value.toObject();tracks.append(row.contains("songInfo")?row.value("songInfo"):row);}
        setSongs(tracks);m_currentPlaylist["count"]=m_songs.size();
        setStatus(QString("%1 · %2 首歌曲").arg(kind=="artist"?"歌手热门歌曲":"专辑").arg(m_songs.size()));
    });
}
void QqMusicBridge::dailyRecommendations(){
    if(m_loggedIn&&beginPage("daily"))return;
    m_currentPlaylist.clear();
    if(!m_loggedIn){setStatus("请先登录 QQ 音乐，再获取每日推荐");return;}cancelRecommendation();const auto revision=++m_listRevision;setStatus("正在获取 QQ 音乐每日推荐…");
    cgi("music.recommend.RecommendFeed","get_recommend_feed",{{"direction",0},{"page",1},{"s_num",0},{"v_cache",QJsonArray()}},[this,revision](const QJsonObject &result){
        if(revision!=m_listRevision)return;if(result.value("code").toInt()!=0){setStatus("QQ 音乐每日推荐读取失败，请稍后重试");return;}
        QString dailyId;std::function<void(const QJsonValue &)> find=[&](const QJsonValue &value){
            if(value.isArray()){for(const auto &child:value.toArray())find(child);return;}if(!value.isObject())return;const auto object=value.toObject();
            if(object.value("subtype").toInt()==510){for(const auto &key:{"id","tid","disstid"})if(idText(object.value(key)).toLongLong()>0){dailyId=idText(object.value(key));break;}}
            if(dailyId.isEmpty())for(auto it=object.begin();it!=object.end();++it)if(it.value().isArray()||it.value().isObject())find(it.value());
        };find(result.value("data"));
        if(dailyId.isEmpty()){setStatus("QQ 音乐暂未返回每日推荐歌单，请稍后重试");return;}loadPlaylist(dailyId);
    });
}
void QqMusicBridge::loadPlaylists(){
    if(m_loggedIn&&beginPage("mine"))return;
    if(!m_loggedIn){setStatus("请先登录 QQ 音乐账号");return;}cancelRecommendation();const auto revision=m_loginRevision,listRevision=++m_listRevision;m_currentPlaylist.clear();setSongs({});
    cgi("music.musicasset.PlaylistBaseRead","GetPlaylistByUin",{{"uin",userId()}},[this,revision,listRevision](const QJsonObject &result){
        if(revision!=m_loginRevision||listRevision!=m_listRevision)return;if(result.value("code").toInt()!=0){setStatus("QQ 音乐歌单读取失败，请稍后重试");return;}
        m_playlists.clear();m_playlists.append(QVariantMap{{"id","liked"},{"name","我喜欢的音乐"},{"cover",""},{"count",0},{"provider","qq"}});
        for(const auto &value:result.value("data").toObject().value("v_playlist").toArray()){const auto p=value.toObject();const auto id=idText(p.value("tid"));if(id.toLongLong()>0&&p.value("dirId").toInt()!=201)m_playlists.append(QVariantMap{{"id",id},{"name",p.value("dirName").toString()},{"cover",p.value("picUrl").toString()},{"count",p.value("songNum").toInt()},{"provider","qq"}});}
        setStatus(QString("读取到 %1 个 QQ 音乐歌单").arg(m_playlists.size()));
    });
}
void QqMusicBridge::recommendedPlaylists(){
    if(beginPage("featured"))return;
    cancelRecommendation();const auto revision=++m_listRevision;m_currentPlaylist.clear();setSongs({});setStatus("正在读取 QQ 音乐推荐歌单…");
    cgi("music.playlist.PlaylistSquare","GetRecommendFeed",{{"From",0},{"Size",24}},[this,revision](const QJsonObject &result){
        if(revision!=m_listRevision)return;m_playlists.clear();for(const auto &value:result.value("data").toObject().value("List").toArray()){const auto p=value.toObject().value("Playlist").toObject().value("basic").toObject();const auto cover=p.value("cover").toObject();m_playlists.append(QVariantMap{{"id",idText(p.value("tid"))},{"name",p.value("title").toString()},{"cover",cover.value("medium_url").toString(cover.value("default_url").toString())},{"count",p.value("song_cnt").toInt()},{"provider","qq"}});}setStatus(m_playlists.isEmpty()?"暂未取得推荐歌单，请重试":"QQ 音乐推荐歌单");
    });
}
struct QqMusicBridge::PlaylistLoad {
    quint64 revision=0;QString id;int offset=0,total=0;QJsonArray songs;QSet<QString> seen;
};
void QqMusicBridge::openPlaylist(const QString &id){
    if((id!="liked"&&id.toLongLong()<=0)||beginPage("playlist:"+id))return;loadPlaylist(id);
}
void QqMusicBridge::loadPlaylist(const QString &id){
    if(id!="liked"&&id.toLongLong()<=0)return;cancelRecommendation();auto load=std::make_shared<PlaylistLoad>();load->revision=++m_listRevision;load->id=id;
    if(!m_pageCache.quiet()){
        m_currentPlaylist={{"id",id},{"provider","qq"},{"name",id=="liked"?"我喜欢的音乐":"QQ 音乐歌单"},{"cover",""},{"count",0}};
        for(const auto &value:m_playlists){const auto p=value.toMap();if(p.value("id").toString()==id){m_currentPlaylist=p;break;}}setSongs({});
    }
    setStatus("正在读取 QQ 音乐歌单…");fetchPlaylistPage(load);
}
void QqMusicBridge::fetchPlaylistPage(const std::shared_ptr<PlaylistLoad> &load){
    if(load->revision!=m_listRevision)return;
    constexpr int pageSize=100;
    cgi("music.srfDissInfo.DissInfo","CgiGetDiss",{{"disstid",load->id=="liked"?0:load->id.toLongLong()},{"dirid",load->id=="liked"?201:0},{"tag",true},{"song_begin",load->offset},{"song_num",pageSize},{"userinfo",true},{"orderlist",true}},[this,load](const QJsonObject &result){
        if(load->revision!=m_listRevision)return;
        if(result.value("code").toInt()!=0){if(m_pageCache.quiet())m_currentPlaylist=m_pageCache.before().value("playlist").toMap();else setSongs(load->songs);setStatus(QString("已读取 %1 首，后续歌曲读取失败，请重新打开歌单重试").arg(m_songs.size()));return;}
        const auto data=result.value("data").toObject();const auto page=data.value("songlist").toArray();load->total=data.value("total_song_num").toInt();const auto before=load->seen.size();
        if(load->offset==0){const auto info=data.value("dirinfo").toObject();if(!info.value("title").toString().isEmpty())m_currentPlaylist["name"]=info.value("title").toString();if(!info.value("picurl").toString().isEmpty())m_currentPlaylist["cover"]=info.value("picurl").toString();m_currentPlaylist["count"]=load->total;}
        for(const auto &value:page){const auto row=song(value.toObject());const auto id=row.value("id").toString();if(id.toLongLong()>0&&!load->seen.contains(id)){load->seen.insert(id);load->songs.append(value);}}
        load->offset+=pageSize;
        const bool more=data.value("hasmore").toVariant().toBool()||load->offset<load->total||(!data.contains("hasmore")&&!load->total&&page.size()==pageSize);
        if(more&&!page.isEmpty()&&load->seen.size()>before){setStatus(QString("正在读取 QQ 音乐歌单 · %1 / %2 首").arg(load->songs.size()).arg(load->total));fetchPlaylistPage(load);return;}
        setSongs(load->songs);setStatus(QString("QQ 音乐歌单 · %1 首歌曲").arg(m_songs.size())+(more?"（其余曲目暂不可读取）":""));
    });
}
void QqMusicBridge::playSong(const QString &id){
    if(!m_catalog.contains(id.toLongLong()))return;QStringList ids;for(const auto &value:m_songs)ids.append(value.toMap().value("id").toString());playSongs(ids,id);
}
void QqMusicBridge::playSongs(const QStringList &ids,const QString &start){
    QVector<Track> tracks;
    for(const auto &id:ids){const auto row=m_catalog.value(id.toLongLong());if(row.isEmpty())continue;Track track;track.id=TrackBase+row.value("id").toLongLong();track.title=row.value("title").toString();track.artist=row.value("artist").toString();track.album=row.value("album").toString();track.duration=row.value("duration").toLongLong();track.hash="qqmusic:"+row.value("mid").toString();
        auto original=row.value("cover").toString();original.replace(QRegularExpression("T002R\\d+x\\d+M000"),"T002M000");
        const auto thumbnail=row.value("thumbnail",QString(original).replace("T002M000","T002R300x300M000")).toString();
        m_artwork->prepare(track,QUrl(thumbnail),QUrl(original),onlinePath(QString("qq-cover-%1.png").arg(row.value("id").toString())));tracks.append(track);}
    m_facade->playOnline(tracks,TrackBase+start.toLongLong());
}
void QqMusicBridge::downloadSong(const QString &id){
    auto row=m_catalog.value(id.toLongLong());row["provider"]="qq";if(!row.value("id").toString().isEmpty())resolveDownload(row,m_quality);
}
void QqMusicBridge::resolveDownload(const QVariantMap &song,const QString &quality){
    const auto row=song;
    const auto mid=row.value("mid").toString(),media=row.value("mediaMid").toString(),guid=QString::number(QRandomGenerator::global()->generate());
    const auto prefix=quality=="hires"?QString("RS01"):quality=="lossless"?QString("F000"):quality=="exhigh"?QString("M800"):QString("M500");const auto format=quality=="hires"||quality=="lossless"?QString("flac"):QString("mp3");
    cgi("music.vkey.GetVkey","UrlGetVkey",{{"guid",guid},{"uin",userId().isEmpty()?"0":userId()},{"filename",QJsonArray{prefix+(media.isEmpty()?mid+mid:media)+"."+format}},{"songmid",QJsonArray{mid}},{"songtype",QJsonArray{row.value("songType").toInt()}},{"ctx",0},{"loginflag",1},{"platform","20"},{"xcdn",1}},[this,row,mid,guid,format](const QJsonObject &result)mutable{
        const auto data=result.value("data").toObject();const auto entries=data.value("midurlinfo").toArray();const auto path=entries.isEmpty()?QString():entries.first().toObject().value("purl").toString();
        if(result.value("code").toInt()!=0||path.isEmpty()){const QString error="当前 QQ 音乐账号未取得这首歌的下载资源";if(!row.contains("_downloadTask"))setStatus(error);emit downloadFailed(row,error);return;}
        auto ready=[this,row,mid,path,format](const QJsonArray &servers)mutable{QUrl url(path);if(url.isRelative()){if(servers.isEmpty()){emit downloadFailed(row,"QQ 音乐未返回下载服务器，请重试");return;}url=QUrl(servers.first().toString()).resolved(url);}row["provider"]="qq";row["key"]="qqmusic:"+mid;emit downloadReady(row,url,format);};
        const auto servers=data.value("sip").toArray();if(QUrl(path).isRelative()&&servers.isEmpty())cgi("CDN.SrfCdnDispatchServer","GetCdnDispatch",{{"guid",guid},{"uid","0"},{"use_new_domain",1},{"use_ipv6",1}},[ready=std::move(ready)](const QJsonObject &dispatch)mutable{ready(dispatch.value("data").toObject().value("sip").toArray());});else ready(servers);
    });
}
void QqMusicBridge::requestSource(qint64 id){
    if(id<TrackBase||id>=TrackBase+OnlineMusicBridge::TrackBase)return;const auto row=m_catalog.value(id-TrackBase);if(row.isEmpty())return;
    const auto revision=++m_playRevision;const auto mid=row.value("mid").toString(),media=row.value("mediaMid").toString();
    const auto guid=QString::number(QRandomGenerator::global()->generate());
    const auto file=m_quality=="hires"?QString("RS01"):m_quality=="lossless"?QString("F000"):m_quality=="exhigh"?QString("M800"):QString("M500");const auto extension=m_quality=="hires"||m_quality=="lossless"?".flac":".mp3";
    setStatus("正在获取 QQ 音乐播放地址…");
    cgi("music.vkey.GetVkey","UrlGetVkey",{{"guid",guid},{"uin",userId().isEmpty()?"0":userId()},{"filename",QJsonArray{file+(media.isEmpty()?mid+mid:media)+extension}},
        {"songmid",QJsonArray{mid}},{"songtype",QJsonArray{row.value("songType").toInt()}},{"ctx",0},{"loginflag",1},{"platform","20"},{"xcdn",1}},[this,id,revision,guid](const QJsonObject &result){
        if(revision!=m_playRevision||!m_facade->onlineActive()||m_facade->current().id!=id)return;
        const auto data=result.value("data").toObject();const auto entries=data.value("midurlinfo").toArray();const auto entry=entries.isEmpty()?QJsonObject():entries.first().toObject();const auto path=entry.value("purl").toString();
        trace(QString("playback code=%1 result=%2 urlPresent=%3").arg(result.value("code").toInt()).arg(entry.value("result").toInt()).arg(!path.isEmpty()));
        if(result.value("code").toInt()!=0||path.isEmpty()){
            const auto message=!m_loggedIn?QString("请先登录 QQ 音乐账号，再播放这首歌"):QString("当前 QQ 音乐账号或所选音质暂无播放权限，可切换音质或检查会员状态");setStatus(message);m_facade->failOnlineSource(id,message);return;
        }
        auto start=[this,id,revision,path](const QJsonArray &servers){
            if(revision!=m_playRevision||!m_facade->onlineActive()||m_facade->current().id!=id)return;
            QUrl url(path);if(url.isRelative()){if(servers.isEmpty()){setStatus("QQ 音乐未返回播放服务器，请稍后重试");m_facade->failOnlineSource(id,m_status);return;}url=QUrl(servers.first().toString()).resolved(url);}
            if(url.scheme()!="https"&&url.scheme()!="http"){setStatus("QQ 音乐播放地址无效，请稍后重试");m_facade->failOnlineSource(id,m_status);return;}
            setStatus("正在播放 · QQ 音乐");m_facade->setOnlineSource(id,url);loadExtras(id);
        };
        const auto servers=data.value("sip").toArray();
        if(QUrl(path).isRelative()&&servers.isEmpty()){
            cgi("CDN.SrfCdnDispatchServer","GetCdnDispatch",{{"guid",guid},{"uid","0"},{"use_new_domain",1},{"use_ipv6",1}},[start=std::move(start)](const QJsonObject &dispatch){start(dispatch.value("data").toObject().value("sip").toArray());});
        }else start(servers);
    });
}
void QqMusicBridge::loadExtras(qint64 id){
    const auto row=m_catalog.value(id-TrackBase);const auto mid=row.value("mid").toString();
    cgi("music.musichallSong.PlayLyricInfo","GetPlayLyricInfo",{{"songID",id-TrackBase},{"songMID",mid},{"qrc",0},{"trans",0},{"roma",0},{"crypt",0},{"lrc",1},{"type",0}},[this,id](const QJsonObject &result){
        if(result.value("code").toInt()!=0)return;const auto data=result.value("data").toObject();m_lyrics->setOnlineLyrics(id,(decodeLyric(data.value("lyric").toString())+"\n"+decodeLyric(data.value("trans").toString())).toUtf8());
    });
}
void QqMusicBridge::toggleFavorite(qint64 id){
    if(id<TrackBase||id>=TrackBase+OnlineMusicBridge::TrackBase)return;
    // QQ's current write interface requires a separate signed channel. Do not
    // claim a cloud favorite changed when that operation has not been accepted.
    setStatus(m_loggedIn?"QQ 音乐歌曲收藏暂请在官方客户端操作":"登录 QQ 音乐后可读取我喜欢的音乐");
}
void QqMusicBridge::cancelRecommendation(){if(m_aiTask){m_pageCache.cancelAI();const auto task=m_aiTask;m_aiTask=0;m_ai->requests()->cancel(task);emit changed();}}
void QqMusicBridge::recommend(){
    if(beginPage("ai"))return;
    m_currentPlaylist.clear();
    cancelRecommendation();const auto error=m_ai->validateConfiguration(m_ai->configuration().defaults);if(!error.isEmpty()){setStatus("请先在 AI 设置中配置模型："+error);return;}
    const auto revision=++m_listRevision;setStatus("正在获取 QQ 音乐新歌候选…");
    cgi("newsong.NewSongServer","get_new_song_info",{{"type",5}},[this,revision](const QJsonObject &result){
        if(revision!=m_listRevision)return;if(result.value("code").toInt()!=0){setStatus("QQ 音乐新歌候选读取失败，请稍后重试");return;}setSongs(result.value("data").toObject().value("songlist").toArray());
        if(m_songs.isEmpty()){setStatus("QQ 音乐暂未返回新歌候选，请稍后重试");return;}rankWithAi();
    });
}
void QqMusicBridge::rankWithAi(){
    const auto config=m_ai->configuration().defaults;const auto *model=m_ai->model(config.providerId,config.modelId);
    if(!model){setStatus("AI 模型配置不可用，请检查 AI 设置");return;}
    const auto request=recommendationRequest(config,*model,m_library->tracks(),m_songs);
    if(request.messages.isEmpty()){setStatus("所选 AI 模型的上下文空间不足以生成推荐，请更换模型");return;}
    m_aiRevision=m_listRevision;m_aiTask=m_ai->requests()->complete(config.providerId,request);setStatus("AI 正在结合本地收听记录挑选 QQ 音乐歌曲…");
}
}
