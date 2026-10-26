#pragma once
#include "ui-bridge/qml-shell.h"
#include "lyrics-document-reader.h"
#include "domain/lyric-match.h"
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>
#include <functional>

namespace search_fixture {
using namespace relaxer;
inline QByteArray json(const QJsonArray &a){return QJsonDocument(a).toJson(QJsonDocument::Compact);}
inline QByteArray json(const QJsonObject &a){return QJsonDocument(a).toJson(QJsonDocument::Compact);}
inline QJsonObject record(const Track &t,QString text="Hello world"){
    return {{"id",42},{"trackName",t.title},{"artistName",t.artist},{"albumName",t.album},
        {"duration",t.duration/1000.},{"syncedLyrics","[00:01.00]"+text+"\n[00:02.00]Walk home\n"}};
}
inline Track seed(Library &library,QString hash="one",QString title="Song"){
    Track t;t.title=title;t.artist="Artist";t.album="Album";t.duration=60000;t.hash=hash;
    t.file="Music/"+hash+".wav";t.id=library.insert(t);return library.track(t.id);
}
inline QString provider(const QUrl &url){
    const auto host=url.host();
    if(host.contains("lrclib"))return "lrclib";
    if(host.contains("lyrics.ovh"))return "ovh";
    if(host.contains("163.com"))return "netease";
    if(host.contains("qq.com"))return "qq";
    if(host.contains("kugou"))return "kugou";
    return host;
}
class Reply final:public QNetworkReply {
public:
    Reply(const QNetworkRequest &r,QObject *parent):QNetworkReply(parent){setRequest(r);setUrl(r.url());open(QIODevice::ReadOnly);}
    bool aborted=false;
    void abort()override{if(isFinished())return;aborted=true;setError(OperationCanceledError,"controlled cancel");setFinished(true);emit finished();}
    void answer(int code,QByteArray bytes,NetworkError error=NoError){
        m_bytes=std::move(bytes);m_offset=0;setAttribute(QNetworkRequest::HttpStatusCodeAttribute,code);
        if(error!=NoError)setError(error,"controlled failure");
        else if(code>=400)setError(code>=500?InternalServerError:ContentNotFoundError,"controlled HTTP status");
        setFinished(true);emit readyRead();emit finished();
    }
    qint64 bytesAvailable()const override{return m_bytes.size()-m_offset+QNetworkReply::bytesAvailable();}
protected:
    qint64 readData(char *out,qint64 max)override{const auto count=qMin(max,m_bytes.size()-m_offset);if(count<=0)return -1;memcpy(out,m_bytes.constData()+m_offset,size_t(count));m_offset+=count;return count;}
private:QByteArray m_bytes;qint64 m_offset=0;
};
class Network final:public QNetworkAccessManager {
public:
    QList<QUrl> urls;QList<QPointer<Reply>> replies;
    std::function<void(Reply *)> respond=[](Reply *r){r->answer(200,"[]");};
    QSet<QString> called()const{QSet<QString> out;for(const auto &u:urls)out.insert(provider(u));return out;}
protected:
    QNetworkReply *createRequest(Operation,const QNetworkRequest &r,QIODevice *)override{
        urls.append(r.url());auto *reply=new Reply(r,this);replies.append(reply);
        const auto root=qEnvironmentVariable("RELAXER_D204_EVIDENCE");
        if(!root.isEmpty()){
            QDir().mkpath(root);QFile log(root+"/provider-calls.jsonl");
            if(log.open(QIODevice::Append))log.write(json(QJsonObject{{"test",QString::fromLatin1(QTest::currentTestFunction())},
                {"case",QString::fromLatin1(QTest::currentDataTag())},{"boundary","QNetworkAccessManager::createRequest"},
                {"provider",provider(r.url())},{"url",r.url().toString()}})+'\n');
        }
        if(respond)QTimer::singleShot(0,reply,[this,reply]{respond(reply);});return reply;
    }
};
// Protocol-shaped fixed responses exercise both search and lyric-detail dispatch.
inline void answerProvider(Reply *r,const Track &t,QString text="Hello world"){
    const auto id=provider(r->url()),path=r->url().path();
    const auto lyric=record(t,text)["syncedLyrics"].toString();
    if(id=="lrclib"){r->answer(200,json(QJsonArray{record(t,text)}));return;}
    if(id=="ovh"){r->answer(200,json(QJsonObject{{"lyrics",text+"\nWalk home"}}));return;}
    if(id=="netease"){
        if(path.contains("search"))r->answer(200,json(QJsonObject{{"code",200},{"result",QJsonObject{{"songs",QJsonArray{QJsonObject{
            {"id",42},{"name",t.title},{"artists",QJsonArray{QJsonObject{{"name",t.artist}}}},{"album",QJsonObject{{"name",t.album}}},{"duration",double(t.duration)}}}}}}}));
        else r->answer(200,json(QJsonObject{{"lrc",QJsonObject{{"lyric",lyric}}}}));
    }else if(id=="qq"){
        if(path.contains("search"))r->answer(200,json(QJsonObject{{"code",0},{"data",QJsonObject{{"song",QJsonObject{{"list",QJsonArray{QJsonObject{
            {"mid","song42"},{"name",t.title},{"singer",QJsonArray{QJsonObject{{"name",t.artist}}}},{"albumname",t.album},{"interval",t.duration/1000.}}}}}}}}}));
        else r->answer(200,json(QJsonObject{{"lyric",lyric}}));
    }else if(id=="kugou"){
        if(path.contains("search"))r->answer(200,json(QJsonObject{{"status",200},{"candidates",QJsonArray{QJsonObject{
            {"id","42"},{"accesskey","synthetic"},{"song",t.title},{"singer",t.artist},{"duration",double(t.duration)}}}}}));
        else r->answer(200,json(QJsonObject{{"content",QString::fromLatin1(lyric.toUtf8().toBase64())}}));
    }
}
struct Fixture {
    QTemporaryDir dir;Library lib{dir.filePath("data")};Network net;
    std::unique_ptr<PlaybackFacade> player;std::unique_ptr<LyricsSearchCoordinator> service;
    std::unique_ptr<LyricsSearchBridge> bridge;Track track;bool ok=false;
    Fixture(){
        if(!lib.open())return;track=seed(lib);player=std::make_unique<PlaybackFacade>(&lib,nullptr,true);
        service=std::make_unique<LyricsSearchCoordinator>(&lib,nullptr,QUrl("https://lrclib.net/api"),QUrl(),&net);
        bridge=std::make_unique<LyricsSearchBridge>(&lib,player.get(),service.get());ok=bridge->setTargetTrack(QString::number(track.id));
    }
    ~Fixture(){bridge.reset();service->shutdown();player->shutdown();}
    void sources(const QStringList &ids){for(const auto &s:service->sources())bridge->setSourceEnabled(LyricsSearchCoordinator::sourceId(s),ids.contains(LyricsSearchCoordinator::sourceId(s)));}
    LyricsSearchRequest request()const{LyricsSearchRequest r;r.original=lib.track(track.id);r.match=LyricMatchPolicy::searchTrack(r.original);r.manual=true;r.replace=true;r.enabledSources=bridge->enabledSources();return r;}
};
} // namespace search_fixture
