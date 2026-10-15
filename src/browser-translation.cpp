#include "browser-translation.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QUuid>
#include <QHostAddress>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkProxy>
#include <QRandomGenerator>
#include <QUrlQuery>

namespace relaxer {
namespace {
QString literal(const QString &s){auto bytes=QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);return QString::fromUtf8(bytes.mid(1,bytes.size()-2));}
QString cacheKey(const QString &provider,const QString &language,const QString &text){return provider+'\n'+language+'\n'+text;}
}
BrowserTranslation::BrowserTranslation(const QString &root,QObject *parent):QObject(parent),m_root(QDir(root).absoluteFilePath("browser-translation")){
    m_socket.setProxy(QNetworkProxy::NoProxy);m_timer.setInterval(800);
    connect(&m_timer,&QTimer::timeout,this,&BrowserTranslation::tick);
    connect(&m_socket,&QTcpSocket::connected,this,[this]{
        if(property("traceTest").toBool())qInfo()<<"websocket connected"<<m_socket.peerPort()<<m_path;
        m_key=QUuid::createUuid().toRfc4122().toBase64();
        m_socket.write("GET "+m_path.toUtf8()+" HTTP/1.1\r\nHost: 127.0.0.1:"+QByteArray::number(m_socket.peerPort())+"\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: "+m_key+"\r\nSec-WebSocket-Version: 13\r\n\r\n");
    });
    connect(&m_socket,&QTcpSocket::readyRead,this,&BrowserTranslation::consume);
    connect(&m_socket,&QTcpSocket::disconnected,this,[this]{m_ready=false;if(m_busy&&!m_closing)finish({},"翻译浏览器已关闭，请重试");});
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){if(m_busy)finish({},"无法启动本地 Edge 或 Chrome");});
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{m_ready=false;m_socket.abort();if(m_busy&&!m_closing)finish({},"翻译浏览器已关闭，请重试");});
}
BrowserTranslation::~BrowserTranslation(){cancel();disconnectBrowser();}
QString BrowserTranslation::browserExecutable(){
#ifdef Q_OS_WIN
    for(const auto &base:{qEnvironmentVariable("ProgramFiles(x86)"),qEnvironmentVariable("ProgramFiles"),qEnvironmentVariable("LOCALAPPDATA")}){
        if(base.isEmpty())continue;
        for(const auto &name:{"Microsoft/Edge/Application/msedge.exe","Google/Chrome/Application/chrome.exe"}){const auto path=QDir(base).filePath(name);if(QFileInfo::exists(path))return path;}
    }
#else
    for(const auto &path:{"/usr/bin/microsoft-edge","/usr/bin/google-chrome","/usr/bin/chromium"})if(QFileInfo::exists(path))return path;
#endif
    return {};
}
QUrl BrowserTranslation::pageUrl(const QString &provider,const QString &text,const QString &language){
    if(provider!="baidu")return QUrl("https://www.bing.com/translator");
    QUrl url("https://fanyi.baidu.com/mtpe-individual/transText");QUrlQuery query;
    QString lang=language;if(lang=="ja")lang="jp";else if(lang=="ko")lang="kor";else if(lang=="fr")lang="fra";else if(lang=="es")lang="spa";
    query.addQueryItem("query",text);query.addQueryItem("lang",lang+"2zh");url.setQuery(query);return url;
}
QString BrowserTranslation::readScript(const QString &provider,const QString &text){
    // Read only visible translation DOM; no cookies, tokens, private API or page internals.
    return QString(R"JS((()=>{
      const expected=%1,baidu=%2;
      const visible=e=>e&&e.getClientRects().length&&getComputedStyle(e).visibility!=='hidden';
      const body=document.body?.innerText||'';
      const challenge=[...document.querySelectorAll('iframe')].some(e=>visible(e)&&/captcha|challenge|verify/i.test(e.src)) || /验证您是人类|安全验证|校验失败|人机验证|请完成验证|请通过验证|访问过于频繁|请求过于频繁|异常流量|unusual traffic|verify you are human|too many requests/i.test(body);
      if(challenge)return {attention:'网页需要安全验证或暂时限流，请在浏览器处理后继续'};
      const host=location.hostname;
      if(!(baidu?host==='fanyi.baidu.com':host==='www.bing.com'||host==='cn.bing.com'))return {attention:'请在浏览器完成验证，并返回翻译页面后继续'};
      const input=document.querySelector(baidu?'[data-slate-editor="true"]':'#tta_input_ta');
      if(!input||input.innerText.replace(/\uFEFF/g,'').trim()!==expected.trim())return {state:'waiting-input',actual:input?.innerText,url:location.href};
      let output='';
      if(baidu){
        if(new URL(location.href).searchParams.get('query')!==expected)return {};
        output=[...document.querySelectorAll('[data-sent-id][data-para-id]')].filter(e=>visible(e)&&!input.contains(e)&&e.innerText.trim()!==expected.trim()).map(e=>e.innerText).join('');
        if(!output)output=[...document.querySelectorAll('.target-output,.output-bd .ordinary-output')].filter(visible).map(e=>e.innerText).join('');
      }else{
        const state=window.__relaxerTranslation;
        if(!state||state.source!==expected||!state.changed)return {};
        if(document.querySelector('#tta_tgtsl')?.value!=='zh-Hans')return {};
        output=document.querySelector('#tta_output_ta')?.innerText||'';
      }
      if(/翻译失败|无法翻译|无法连接|请稍后重试|try again later|unable to translate/i.test(output))return {attention:'网页翻译暂时不可用，请检查浏览器页面后继续'};
      output=output.trim();return output&&output!==expected&&/[\u3400-\u9fff]/.test(output)?{text:output}:{state:'waiting-output'};
    })())JS").arg(literal(text),provider=="baidu"?"true":"false");
}
void BrowserTranslation::translate(quint64 token,const QString &provider,const QString &text,const QString &language){
    cancel();m_token=token;m_provider=provider=="baidu"?"baidu":"bing";m_text=text;m_language=language;m_busy=true;m_clock.restart();
    const auto epoch=m_epoch;
    const auto key=cacheKey(m_provider,m_language,m_text);
    if(m_cache.contains(key)){const auto result=m_cache.value(key);QTimer::singleShot(0,this,[this,epoch,result]{if(m_busy&&m_epoch==epoch)finish(result);});return;}
    if(m_text.size()>4000){QTimer::singleShot(0,this,[this,epoch]{if(m_busy&&m_epoch==epoch)finish({},"单行歌词超过 4000 字，请先拆分该行");});return;}
    if(m_ready){submit();m_timer.start();}else launch();
}
void BrowserTranslation::launch(){
    if(m_process.state()!=QProcess::NotRunning){m_timer.start();return;}
    disconnectBrowser();const auto exe=browserExecutable();if(exe.isEmpty()){finish({},"未找到本地 Edge 或 Chrome，请安装其中一个浏览器");return;}
    m_profile=QDir(m_root).absoluteFilePath("profile");if(!QDir().mkpath(m_profile)){finish({},"无法创建翻译浏览器数据目录");return;}
    m_lock=std::make_unique<QLockFile>(QDir(m_root).filePath("session.lock"));
    if(!m_lock->tryLock(0)){m_lock.reset();finish({},"另一个翻译窗口正在使用浏览器，请先关闭它");return;}
    QFile::remove(QDir(m_profile).filePath("DevToolsActivePort"));
    QStringList args{"--user-data-dir="+QDir::toNativeSeparators(m_profile),"--remote-debugging-address=127.0.0.1","--remote-debugging-port=0","--no-first-run","--no-default-browser-check","--disable-breakpad","--disable-crash-reporter","--new-window","about:blank"};
    // Developer smoke tests can run the same owned profile without a visible window.
    if(property("headlessTest").toBool())args.prepend("--headless=new");
    auto environment=QProcessEnvironment::systemEnvironment();
    for(const auto &name:{"TEMP","TMP","LOCALAPPDATA","APPDATA"}){const auto dir=QDir(m_root).filePath(name);QDir().mkpath(dir);environment.insert(name,QDir::toNativeSeparators(dir));}
    m_process.setProcessEnvironment(environment);
    m_process.setProgram(exe);m_process.setArguments(args);if(property("traceTest").toBool())m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);else m_process.setStandardErrorFile(QProcess::nullDevice());m_process.setStandardOutputFile(QProcess::nullDevice());m_process.start();m_timer.start();
}
void BrowserTranslation::disconnectBrowser(){
    m_closing=true;m_ready=false;m_commands.clear();
    if(m_handshake&&m_socket.state()==QAbstractSocket::ConnectedState){command("Browser.close",{}, {},false);m_socket.flush();m_socket.waitForBytesWritten(100);}
    m_socket.abort();m_handshake=false;m_bytes.clear();m_fragments.clear();m_session.clear();m_target.clear();
    if(m_process.state()!=QProcess::NotRunning&&!m_process.waitForFinished(1500)){m_process.kill();m_process.waitForFinished(1500);}
    m_lock.reset();m_closing=false;
}
void BrowserTranslation::cancel(){++m_epoch;m_busy=false;m_waiting=false;m_polling=false;m_submitted=false;m_candidate.clear();m_stable.invalidate();m_timer.stop();}
void BrowserTranslation::resume(){if(!m_busy||!m_waiting)return;m_waiting=false;m_clock.restart();m_candidate.clear();m_stable.invalidate();emit resumed();m_timer.start();}
void BrowserTranslation::showBrowser(){if(m_ready&&!m_target.isEmpty())command("Target.activateTarget",{{"targetId",m_target}}, {},false);}
void BrowserTranslation::finish(const QString &text,const QString &error){
    if(!m_busy)return;const auto token=m_token;if(error.isEmpty()&&!text.isEmpty()){if(m_cache.size()>4096)m_cache.clear();m_cache.insert(cacheKey(m_provider,m_language,m_text),text);}
    cancel();emit completed(token,text,error);
}
void BrowserTranslation::attention(const QString &reason){if(!m_busy||m_waiting)return;m_waiting=true;m_timer.stop();showBrowser();emit needsAttention(reason);}
void BrowserTranslation::tick(){
    if(!m_busy||m_waiting)return;
    if(property("traceTest").toBool()&&m_clock.elapsed()<10000)qInfo()<<"tick"<<m_socket.state()<<m_handshake<<m_ready<<m_polling;
    if(m_clock.elapsed()>90000){attention("等待译文超时，请在浏览器检查登录、验证码或网络，处理后继续");return;}
    if(m_ready){if(!m_polling)poll();return;}
    if(m_socket.state()!=QAbstractSocket::UnconnectedState)return;
    QFile file(QDir(m_profile).filePath("DevToolsActivePort"));if(!file.open(QIODevice::ReadOnly))return;
    const auto lines=file.read(1024).split('\n');bool ok=false;const auto port=lines.value(0).trimmed().toUShort(&ok);m_path=QString::fromUtf8(lines.value(1).trimmed());
    if(!ok||!port||!m_path.startsWith("/devtools/browser/"))return;m_socket.connectToHost(QHostAddress::LocalHost,port);
}
void BrowserTranslation::frame(int opcode,const QByteArray &payload){
    QByteArray bytes;bytes+=char(0x80|opcode);const quint64 size=quint64(payload.size());
    if(size<126)bytes+=char(0x80|size);else if(size<=65535){bytes+=char(0x80|126);bytes+=char(size>>8);bytes+=char(size);}else{bytes+=char(0x80|127);for(int i=7;i>=0;--i)bytes+=char(size>>(8*i));}
    const quint32 mask=QRandomGenerator::global()->generate();QByteArray maskBytes;for(int i=0;i<4;++i)maskBytes+=char(mask>>(8*i));bytes+=maskBytes;
    for(qsizetype i=0;i<payload.size();++i)bytes+=char(uchar(payload[i])^uchar(maskBytes[i%4]));m_socket.write(bytes);
}
void BrowserTranslation::command(const QString &method,const QJsonObject &params,std::function<void(const QJsonObject &)> callback,bool session){
    const int serial=++m_serial;QJsonObject data{{"id",serial},{"method",method},{"params",params}};
    if(property("traceTest").toBool())qInfo()<<"CDP send"<<serial<<method;
    if(session&&!m_session.isEmpty())data["sessionId"]=m_session;
    if(callback)m_commands.insert(serial,std::move(callback));frame(1,QJsonDocument(data).toJson(QJsonDocument::Compact));
}
void BrowserTranslation::consume(){
    m_bytes+=m_socket.readAll();if(m_bytes.size()>4*1024*1024){finish({},"浏览器响应过大");m_socket.abort();return;}
    if(!m_handshake){
        const auto end=m_bytes.indexOf("\r\n\r\n");if(end<0)return;
        const auto header=m_bytes.left(end);const auto expected=QCryptographicHash::hash(m_key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",QCryptographicHash::Sha1).toBase64();
        if(!header.startsWith("HTTP/1.1 101 ")||!header.contains(expected)){finish({},"无法连接本地翻译浏览器");m_socket.abort();return;}
        m_bytes.remove(0,end+4);m_handshake=true;
        command("Target.createTarget",{{"url","about:blank"}},[this](const QJsonObject &reply){
            m_target=reply["result"].toObject()["targetId"].toString();if(m_target.isEmpty()){finish({},"无法创建翻译标签页");return;}
            command("Target.attachToTarget",{{"targetId",m_target},{"flatten",true}},[this](const QJsonObject &attached){
                m_session=attached["result"].toObject()["sessionId"].toString();if(m_session.isEmpty()){finish({},"无法控制翻译标签页");return;}
                m_ready=true;if(m_busy)submit();
            },false);
        },false);
    }
    while(m_bytes.size()>=2){
        const int first=uchar(m_bytes[0]),second=uchar(m_bytes[1]);int offset=2;quint64 length=second&127;
        if(second&128){finish({},"浏览器通信协议异常");m_socket.abort();return;}
        if(length==126){if(m_bytes.size()<4)return;length=(uchar(m_bytes[2])<<8)|uchar(m_bytes[3]);offset=4;}
        else if(length==127){if(m_bytes.size()<10)return;length=0;for(int i=2;i<10;++i)length=(length<<8)|uchar(m_bytes[i]);offset=10;}
        if(length>2*1024*1024){finish({},"浏览器响应过大");m_socket.abort();return;}if(quint64(m_bytes.size())<length+offset)return;
        const auto payload=m_bytes.mid(offset,qsizetype(length));m_bytes.remove(0,offset+qsizetype(length));const int opcode=first&15;
        if(opcode==9){frame(10,payload);continue;}if(opcode==10)continue;if(opcode==8){m_socket.disconnectFromHost();return;}
        if(opcode==1){m_fragments=payload;m_fragmentOpcode=1;}else if(opcode==0&&m_fragmentOpcode==1)m_fragments+=payload;else continue;
        if(m_fragments.size()>2*1024*1024){finish({},"浏览器响应过大");m_socket.abort();return;}if(!(first&128))continue;
        const auto message=QJsonDocument::fromJson(m_fragments).object();m_fragments.clear();m_fragmentOpcode=0;
        if(property("traceTest").toBool()&&message.contains("id"))qInfo()<<"CDP reply"<<message["id"]<<message["error"];
        auto callback=m_commands.take(message["id"].toInt());if(callback)callback(message);
    }
}
void BrowserTranslation::evaluate(const QString &script,std::function<void(const QJsonObject &)> callback){
    const auto epoch=m_epoch;
    command("Runtime.evaluate",{{"expression",script},{"returnByValue",true}},[this,epoch,callback=std::move(callback)](const QJsonObject &reply){
        if(!m_busy||epoch!=m_epoch)return;
        if(property("traceTest").toBool())qInfo().noquote()<<QJsonDocument(reply).toJson(QJsonDocument::Compact);
        callback(reply["result"].toObject()["result"].toObject()["value"].toObject());
    });
}
void BrowserTranslation::submit(){
    if(!m_busy)return;m_polling=true;const auto epoch=m_epoch;m_submitted=false;
    command("Page.navigate",{{"url",pageUrl(m_provider,m_text,m_language).toString(QUrl::FullyEncoded)}},[this,epoch](const QJsonObject &reply){
        if(!m_busy||epoch!=m_epoch)return;m_polling=false;
        if(reply.contains("error")||!reply["result"].toObject()["errorText"].toString().isEmpty())attention("无法加载翻译网页，请检查浏览器后继续");
    });
}
void BrowserTranslation::poll(){
    m_polling=true;
    if(m_provider=="bing"&&!m_submitted){
        const auto script=QString(R"JS((()=>{
          if(!['www.bing.com','cn.bing.com'].includes(location.hostname))return {};
          const input=document.querySelector('#tta_input_ta'),output=document.querySelector('#tta_output_ta'),src=document.querySelector('#tta_srcsl'),dst=document.querySelector('#tta_tgtsl');
          if(!input||!output||!src||!dst)return {};
          const state={source:%1,changed:false};window.__relaxerTranslation?.observer?.disconnect();window.__relaxerTranslation=state;
          state.observer=new MutationObserver(()=>{state.changed=true;});state.observer.observe(output,{subtree:true,childList:true,characterData:true});
          src.value=[...src.options].some(o=>o.value===%2)?%2:'auto-detect';src.dispatchEvent(new Event('change',{bubbles:true}));
          dst.value='zh-Hans';dst.dispatchEvent(new Event('change',{bubbles:true}));
          input.innerText='';input.focus();return {prepared:true};
        })())JS").arg(literal(m_text),literal(m_language));
        evaluate(script,[this](const QJsonObject &state){
            if(!state["prepared"].toBool()){m_polling=false;return;}
            const auto epoch=m_epoch;command("Input.insertText",{{"text",m_text}},[this,epoch](const QJsonObject &reply){if(!m_busy||m_epoch!=epoch)return;m_polling=false;m_submitted=!reply.contains("error");});
        });return;
    }
    evaluate(readScript(m_provider,m_text),[this](const QJsonObject &result){
        m_polling=false;const auto reason=result["attention"].toString();if(!reason.isEmpty()){attention(reason);return;}
        const auto text=result["text"].toString();if(text.isEmpty()){m_candidate.clear();m_stable.invalidate();return;}
        if(text!=m_candidate){m_candidate=text;m_stable.restart();return;}
        if(m_stable.isValid()&&m_stable.elapsed()>=1600)finish(text);
    });
}
}
