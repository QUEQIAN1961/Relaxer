#include "search-parameters-fixture.h"
#include "lyrics-replacement-fixture.h"
#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTextStream>

// Standalone controlled-provider GUI for independent Harness input. Never opens
// an existing data root and never delegates a request to a real network manager.
int main(int argc,char **argv){
    QApplication app(argc,argv);QCommandLineParser args;args.addHelpOption();
    args.addOption({"data","New or empty synthetic data directory","path"});
    args.addOption({"evidence","Provider-call JSONL directory","path"});
    args.addOption({"fail-source","Controlled failing source ID (e.g. ovh)","id"});
    args.addOption({"delay-ms","Controlled response delay in milliseconds","ms","100"});
    args.addOption({"quit-after","Automatic startup/exit smoke only","ms","0"});args.process(app);
    const auto root=QDir(args.value("data")).absolutePath();
    if(args.value("data").isEmpty()||QDir(root).entryList(QDir::NoDotAndDotDot|QDir::AllEntries|QDir::Hidden|QDir::System).size()){
        QTextStream(stderr)<<"--data must name a new or empty isolated directory\n";return 64;
    }
    if(!args.value("evidence").isEmpty())qputenv("RELAXER_D204_EVIDENCE",args.value("evidence").toUtf8());
    Library lib(root);if(!lib.open())return 65;
    const auto a=search_fixture::seed(lib,"clean","Song"),b=search_fixture::seed(lib,"bilingual","Song (Live)"),c=search_fixture::seed(lib,"third","第二首 Song");
    for(const auto &t:{a,b,c})if(!t.id||!c4c5_fixture::wave(lib.absolute(t.file)))return 66;
    if(!lib.saveLyricsPreservingText(b.id,replacement_fixture::bilingual()))return 67;
    search_fixture::Network network;
    const auto failure=args.value("fail-source");const auto delay=qBound(0,args.value("delay-ms").toInt(),60000);
    network.respond=[&](search_fixture::Reply *reply){
        const auto t=lib.track(reply->property("lookupTrackId").toLongLong());
        QTimer::singleShot(delay,reply,[reply,t,failure]{
            if(reply->isFinished())return;
            if(search_fixture::provider(reply->url())==failure)reply->answer(0,{},QNetworkReply::ConnectionRefusedError);
            else search_fixture::answerProvider(reply,t);
        });
    };
    QmlShell shell(&lib,nullptr,true,nullptr,{}, {},&network);if(!shell.load())return 68;
    auto *bridge=shell.lyricsSearchBridge();if(!bridge->setTargetTracks({QString::number(a.id),QString::number(b.id),QString::number(c.id)},false,true))return 69;
    auto *workspace=c4c5_fixture::visual(shell.window()->contentItem(),"lyricsWorkspace");if(!workspace||!QMetaObject::invokeMethod(workspace,"open"))return 70;
    const auto quitAfter=args.value("quit-after").toInt();if(quitAfter>0)QTimer::singleShot(quitAfter,&app,&QApplication::quit);
    QTextStream(stdout)<<"Controlled provider fixture; data="<<root<<"; no real provider calls\n";
    const int result=app.exec();return shell.shutdown()?result:71;
}
