#include "artwork-recovery-fixture.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
using namespace artwork_fixture;
// Creates only a new/empty synthetic root. It never launches a UI or recovery.
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QCommandLineParser args;args.addHelpOption();
    args.addOption({"data","New or empty isolated synthetic library root","path"});
    args.addOption({"old-schema","Remove cover_origin to exercise historical schema upgrade"});args.process(app);
    const auto root=QDir(args.value("data")).absolutePath();
    if(args.value("data").isEmpty()||!QDir(root).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty()){
        QTextStream(stderr)<<"--data must identify a new or empty isolated directory\n";return 64;
    }
    Library l(root);if(!l.open())return 65;const auto inputs=l.absolute("FixtureSources");if(!QDir().mkpath(inputs))return 66;
    QJsonArray rows;QVector<qint64> ids;
    auto record=[&](const QString &name,const Track &track,const QString &expected){
        if(!track.id)return false;ids<<track.id;
        rows.append(QJsonObject{{"case",name},{"trackKey",QString::number(track.id)},{"hash",track.hash},{"file",track.file},
            {"cover",track.cover},{"origin",track.coverOrigin},{"expected",expected},
            {"musicSha256",QString::fromLatin1(QCryptographicHash::hash(bytes(l.absolute(track.file)),QCryptographicHash::Sha256).toHex())},
            {"oldCoverSha256",QString::fromLatin1(QCryptographicHash::hash(bytes(l.absolute(track.cover)),QCryptographicHash::Sha256).toHex())}});return true;
    };
    const auto a=legacy(l,embedded(inputs,"red-valid-legacy",Qt::red,440));
    const auto b=legacy(l,embedded(inputs,"green-missing-legacy",Qt::green,550),QByteArray());
    const auto c=legacy(l,embedded(inputs,"magenta-corrupt-legacy",Qt::magenta,660),"invalid cache bytes");
    if(!record("valid-legacy-embedded",a,"embedded red; keep old blue cache bytes")||
       !record("missing-legacy-embedded",b,"embedded green")||!record("corrupt-legacy-embedded",c,"embedded magenta"))return 67;
    auto plain=[&](const QString &name,int tone){const auto p=QDir(inputs).filePath(name+".wav");return ffmpeg({"-f","lavfi","-i",QString("sine=frequency=%1:duration=0.2").arg(tone),p})?p:QString();};
    const auto d=legacy(l,plain("no-source",770));const auto e=legacy(l,plain("missing-source",880),QByteArray());
    if(!record("no-source-keep-valid",d,"preserve valid blue old cover; one persistent no-source receipt")||
       !record("no-source-placeholder",e,"placeholder; no false recovered count"))return 68;
    auto f=Importer::importOne(root,plain("manual",990));if(!f.error.isEmpty())return 69;f.track.id=l.insert(f.track);const auto manual=QDir(inputs).filePath("manual.png");
    if(!write(manual,png(Qt::yellow))||!l.updateArtwork(f.track.id,manual)||!record("manual-valid",l.track(f.track.id),"manual yellow unchanged"))return 70;
    auto g=Importer::importOne(root,plain("manual-missing",1000));if(!g.error.isEmpty())return 71;g.track.id=l.insert(g.track);
    if(!l.updateArtwork(g.track.id,manual)||!QFile::remove(l.absolute(l.track(g.track.id).cover))||!record("manual-missing",l.track(g.track.id),"manual identity unchanged; placeholder"))return 72;
    auto h=Importer::importOne(root,embedded(inputs,"current-cyan",Qt::cyan,1100));if(!h.error.isEmpty())return 73;h.track.id=l.insert(h.track);
    if(!record("current-valid",l.track(h.track.id),"existing current cache unchanged; old-schema migration classifies nonlegacy as manual"))return 74;
    const auto list=l.createPlaylist("D2-05 synthetic same-title covers");if(!list||!l.addToPlaylist(list,ids))return 75;
    if(!l.saveLyricsPreservingText(a.id,"[00:01.00]Synthetic preserved lyrics\n")||!l.favorite(a.id,true))return 76;
    auto session=l.startSession(a.id,1000);if(!l.saveSession(session,800,80))return 77;
    if(!l.setSettings({{"closeToTray",false},{"reduceMotion",true},{"playerCoverBackground",true}}))return 78;
    if(args.isSet("old-schema")){QSqlQuery q(import_fixture::database(l));if(!q.exec("ALTER TABLE tracks DROP COLUMN cover_origin"))return 79;}
    const QJsonObject manifest{{"fixture","D2-05 synthetic only"},{"oldSchema",args.isSet("old-schema")},{"tracks",rows},
        {"expectedRecoveryCount",3},{"expectedNoSource",2},{"playlistKey",QString::number(list)},{"realUserDataTouched",false}};
    if(!write(l.absolute("fixture-manifest.json"),QJsonDocument(manifest).toJson(QJsonDocument::Indented)))return 80;
    QTextStream(stdout)<<"Created 8 synthetic tracks, 3 recoverable legacy covers; root="<<root<<"\n";return 0;
}
