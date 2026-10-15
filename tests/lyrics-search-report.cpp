#include "importer.h"
#include "lyricslookup.h"
#include "lookup-drive.h"
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
using namespace relaxer;

// Explicit, local diagnostic: use temporary libraries, never the user's data.
// Reports contain query/metadata/error information, never full lyric text.
static QJsonObject runRound(const QStringList &files,bool manual,bool envProxy) {
    QTemporaryDir temporary;
    Library library(temporary.filePath("data"));
    QJsonObject result{{"mode",manual?"manual":"batch"}};
    if(!library.open()){result["error"]=library.lastError();return result;}
    library.setSetting("translateLyrics",false);
    QVector<qint64> ids;QJsonArray imports;
    for(const auto &path:files){
        const auto imported=Importer::importOne(library.root(),path);
        if(!imported.error.isEmpty()){result["error"]=QFileInfo(path).fileName()+": "+imported.error;return result;}
        const auto id=library.insert(imported.track);
        if(!id){result["error"]=library.lastError();return result;}
        ids<<id;imports<<QJsonObject{{"id",id},{"file",QFileInfo(path).fileName()},{"title",imported.track.title},{"artist",imported.track.artist},{"album",imported.track.album},{"duration_ms",imported.track.duration}};
    }
    result["imports"]=imports;
    LyricsLookup dialog(&library,ids,nullptr,QUrl("https://lrclib.net/api"),{},true);
    dialog.setProperty("skipLyricTranslation",true);
    dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
    auto *network=dialog.findChild<QNetworkAccessManager *>();
    if(envProxy){
        QTest::qWait(100);
        QString configured=qEnvironmentVariable("https_proxy");if(configured.isEmpty())configured=qEnvironmentVariable("HTTPS_PROXY");
        const QUrl proxy(configured);
        if((proxy.scheme()=="http"||proxy.scheme()=="https")&&!proxy.host().isEmpty())network->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy,proxy.host(),proxy.port(8080)));
    }
    QElapsedTimer elapsed;elapsed.start();QJsonArray requests;int selected=-1;qint64 notBefore=0;
    QObject::connect(network,&QNetworkAccessManager::finished,&dialog,[&](QNetworkReply *reply){
        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==429)notBefore=qMax(notBefore,QDateTime::currentMSecsSinceEpoch()+1000*qBound(30,reply->rawHeader("Retry-After").toInt(),600));
        QUrl url=reply->url();url.setUserInfo({});QUrlQuery query(url);query.removeAllQueryItems("accesskey");url.setQuery(query);
        requests<<QJsonObject{{"finished_ms",elapsed.elapsed()},{"track_id",reply->property("lookupTrackId").toLongLong()},{"search_pass",reply->property("lookupSearchPass").toInt()},{"manual_row",selected},{"url",url.toString()},{"http",reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()},{"network_error",int(reply->error())},{"timed_out",reply->property("lookupTimedOut").toBool()}};
    });
    auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");
    auto *start=dialog.findChild<QPushButton *>("lyricsStart");
    const auto waitDone=[&](int limit){QElapsedTimer deadline;deadline.start();while(!start->isEnabled()&&deadline.elapsed()<limit)QTest::qWait(30);return start->isEnabled();};
    bool completed=true;
    if(manual){
        // This is a fresh library: batch results/cache cannot inflate manual results.
        for(int row=0;row<rows->count();++row){
            selected=row;rows->setCurrentRow(row);dialog.findChild<QPushButton *>("lyricsManualSearch")->click();
            if(!waitDone(40000)){completed=false;break;}
        }
    }else{
        rows->clearSelection();relaxerClickSearch(&dialog);
        completed=waitDone((files.size()+2)*65000);
    }
    if(!completed)dialog.findChild<QPushButton *>("lyricsStop")->click();
    QJsonArray states;
    for(int row=0;row<rows->count();++row){
        rows->setCurrentRow(row);const auto id=rows->item(row)->data(Qt::UserRole).toLongLong();const auto track=library.track(id);QJsonArray records;
        for(const auto &value:library.lookupCache(id).records){
            const auto record=value.toObject();auto text=record["syncedLyrics"].toString();if(text.isEmpty())text=record["plainLyrics"].toString();
            records<<QJsonObject{{"title",record["trackName"]},{"artist",record["artistName"]},{"album",record["albumName"]},{"duration_seconds",record["duration"]},{"source",record["source"]},{"instrumental",record["instrumental"].toBool()},{"synced",!record["syncedLyrics"].toString().isEmpty()},{"lyrics_sha256",QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256).toHex())}};
        }
        states<<QJsonObject{{"id",id},{"title",track.title},{"saved",!track.lyrics.isEmpty()},{"status",rows->item(row)->data(Qt::UserRole+2).toString()},{"state",rows->item(row)->data(Qt::UserRole+1).toInt()},{"candidate_count",dialog.findChild<QListWidget *>("lyricsVersions")->count()},{"candidates",records}};
    }
    result["completed"]=completed;result["elapsed_ms"]=elapsed.elapsed();result["not_before_utc_ms"]=notBefore;result["requests"]=requests;result["tracks"]=states;
    return result;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser;parser.setApplicationDescription("Relaxer lyrics search diagnostic (temporary library; explicit online requests)");parser.addHelpOption();
    parser.addOption({"input-dir","Read up to 20 MP3/FLAC/WAV files from this directory (no recursion).","directory"});
    parser.addOption({"output","Write the UTF-8 JSON report here.","path","lyrics-search-report.json"});
    parser.addOption({"compare-manual","Also run unchanged manual searches in a separate fresh temporary library."});
    parser.addOption({"proxy-from-env","Use the HTTP proxy in https_proxy/HTTPS_PROXY for this diagnostic only."});
    parser.addPositionalArgument("files","Audio files to test (up to 20).","[files...]");parser.process(app);
    QStringList files=parser.positionalArguments();
    if(parser.isSet("input-dir")){
        const QDir directory(parser.value("input-dir"));
        if(!directory.exists()){qCritical()<<"Input directory does not exist";return 2;}
        for(const auto &name:directory.entryList({"*.mp3","*.flac","*.wav"},QDir::Files,QDir::Name))files<<directory.absoluteFilePath(name);
    }
    for(auto &path:files)path=QFileInfo(path).absoluteFilePath();files.removeDuplicates();
    if(files.isEmpty()||files.size()>20){qCritical()<<"Specify 1 to 20 audio files";return 2;}
    for(const auto &path:files)if(!QFileInfo(path).isFile()){qCritical()<<"Missing input:"<<QFileInfo(path).fileName();return 2;}
    const auto outputPath=QFileInfo(parser.value("output")).absoluteFilePath();
    if(files.contains(outputPath)){qCritical()<<"Report path must differ from input audio";return 2;}
    QJsonArray rounds;bool success=true;qint64 notBefore=0;
    for(bool manual:{false,true}){
        if(manual&&!parser.isSet("compare-manual"))continue;
        if(manual)while(QDateTime::currentMSecsSinceEpoch()<notBefore)QTest::qWait(50);
        qInfo()<<"Starting"<<(manual?"manual comparison":"batch search")<<files.size()<<"files";
        const auto round=runRound(files,manual,parser.isSet("proxy-from-env"));rounds<<round;notBefore=qint64(round["not_before_utc_ms"].toDouble());
        success&=round["completed"].toBool()&&!round.contains("error");
    }
    const QJsonObject report{{"schema",1},{"created_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"state_codes",QJsonObject{{"waiting",0},{"searching",1},{"complete",2},{"needs_choice",3},{"failed",4}}},{"rounds",rounds}};
    QSaveFile output(parser.value("output"));const auto bytes=QJsonDocument(report).toJson();
    if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit()){qCritical()<<"Cannot save report";return 2;}
    qInfo().noquote()<<QFileInfo(parser.value("output")).absoluteFilePath();return success?0:3;
}
