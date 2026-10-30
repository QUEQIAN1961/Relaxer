#include <QtTest>
#include "artwork-recovery-fixture.h"
#include "artwork-diagnostics.h"
#include <QDirIterator>
#include <QJsonDocument>
#include <QSqlRecord>
#include <QSqlDriver>
#include <QScopeGuard>
#include <sqlite3.h>
#include <thread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace relaxer;
using namespace artwork_fixture;
namespace {
QString digest(const QByteArray &b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());}
QJsonObject state(Library &l,qint64 id,qint64 list) {
    QJsonObject files;QDirIterator it(l.root(),QDir::Files,QDirIterator::Subdirectories);
    while(it.hasNext()){const auto path=it.next();const auto rel=QDir(l.root()).relativeFilePath(path);
        if(rel.startsWith("Music/")||rel.startsWith("Lyrics/")||rel.startsWith("Artwork/"))files[rel]=digest(bytes(path));}
    QJsonObject row;QSqlQuery q(import_fixture::database(l));q.prepare("SELECT * FROM tracks WHERE id=?");q.addBindValue(id);
    if(q.exec()&&q.next())for(int i=0;i<q.record().count();++i)row[q.record().fieldName(i)]=QJsonValue::fromVariant(q.value(i));
    QJsonArray members;for(const auto &t:l.tracks(list))members.append(QString::number(t.id));
    return {{"filesSHA256",files},{"sqliteTrack",row},{"playlistMembers",members}};
}
void evidence(const QString &name,const QJsonObject &value){
    const auto dir=qEnvironmentVariable("RELAXER_P2_EVIDENCE");if(dir.isEmpty())return;
    QDir().mkpath(dir);write(QDir(dir).filePath(name+".json"),QJsonDocument(value).toJson());
}
bool captureDatabase(Library &l,const QString &suffix){
    const auto dir=qEnvironmentVariable("RELAXER_P2_EVIDENCE");if(dir.isEmpty())return true;
    QDir().mkpath(dir);auto path=QDir(dir).filePath(QString::fromLatin1(QTest::currentTestFunction())+"-"+QString::fromLatin1(QTest::currentDataTag()?QTest::currentDataTag():"single")+"-"+suffix+".sqlite");
    // sqlite's own snapshot handles journal/WAL state; never copy a live DB.
    path.replace("'","''");QSqlQuery q(import_fixture::database(l));return q.exec("VACUUM INTO '"+path+"'");
}
}
class ArtworkIntegrityTests final:public QObject {
    Q_OBJECT
private slots:
    void checkedCommit_data(){
        QTest::addColumn<QString>("change");QTest::addColumn<bool>("accept");
        QTest::newRow("P2-01-unchanged")<<QString("unchanged")<<true;
        QTest::newRow("P2-02-length")<<QString("length")<<false;
        QTest::newRow("P2-03-mtime")<<QString("mtime")<<false;
        QTest::newRow("P2-04-equal-length")<<QString("equal-length")<<false;
        QTest::newRow("P2-05-equal-length-restored-mtime")<<QString("restored-mtime")<<false;
        QTest::newRow("P2-06-deleted")<<QString("deleted")<<false;
        QTest::newRow("P2-07-unreadable")<<QString("unreadable")<<false;
        QTest::newRow("P2-08-manual")<<QString("manual")<<false;
        QTest::newRow("P2-10-database")<<QString("database")<<false;
        QTest::newRow("P2-11-cache")<<QString("cache")<<false;
        QTest::newRow("P2-12-same-content-new-object")<<QString("new-object")<<false;
    }
    void checkedCommit(){
        QFETCH(QString,change);QFETCH(bool,accept);
        QJsonObject diagnostic;QString temporaryRoot,connectionName;bool preserve=false;
        const auto report=qScopeGuard([&]{
            diagnostic["temporaryRoot"]=temporaryRoot;diagnostic["preservedOnFailure"]=preserve;
            diagnostic["directoryExistsAfterTemporaryLifetime"]=QFileInfo::exists(temporaryRoot);
            evidence(QString::fromLatin1(QTest::currentDataTag())+"-creation-diagnostic",diagnostic);
        });
        QTemporaryDir d;QVERIFY(d.isValid());temporaryRoot=d.path();
        diagnostic["directoryCreatedUTCms"]=double(QDateTime::currentMSecsSinceEpoch());
        const auto closed=qScopeGuard([&]{
            diagnostic["libraryDestroyedUTCms"]=double(QDateTime::currentMSecsSinceEpoch());
            diagnostic["connectionPresentAfterLibraryDestruction"]=QSqlDatabase::connectionNames().contains(connectionName);
            diagnostic["databaseExistsAfterClose"]=QFileInfo::exists(QDir(temporaryRoot).filePath("data/relaxer.sqlite"));
        });
        Library l(d.filePath("data"));QVERIFY(l.open());connectionName=import_fixture::database(l).connectionName();
        diagnostic["opened"]=artwork_diagnostics::databaseState(l,"opened");
        auto red=bytes(embedded(d.path(),"red",Qt::red)),green=bytes(embedded(d.path(),"green",Qt::green));QVERIFY(!red.isEmpty()&&!green.isEmpty());
        const auto size=qMax(red.size(),green.size());red.resize(size,'\0');green.resize(size,'\0');QVERIFY(red!=green);
        const auto source=d.filePath("padded.mp3");QVERIFY(write(source,red));QJsonObject creation;
        auto t=artwork_diagnostics::legacy(l,source,creation);diagnostic["creation"]=creation;
        if(!t.id){preserve=true;d.setAutoRemove(false);qWarning().noquote()<<"ARTWORK_CREATION_FAILURE"<<QJsonDocument(creation).toJson(QJsonDocument::Compact);}
        QVERIFY(t.id);
        QVERIFY(l.saveLyricsPreservingText(t.id,"[00:01.00]Keep original lyric\n"));QVERIFY(l.favorite(t.id,true));
        const auto list=l.createPlaylist("Integrity");QVERIFY(l.addToPlaylist(list,{t.id}));t=l.track(t.id);
        const auto r=Importer::repairArtwork(l.root(),t);QCOMPARE(r.outcome,ArtworkOutcome::Recovered);QVERIFY(closeColor(decodeRecoveredArtwork(r.png),Qt::red));
        const auto sourcePath=l.absolute(t.file),target=l.absolute("Artwork/"+t.hash+"-v2-"+digest(r.png)+".png");
        if(change=="length"){QFile f(sourcePath);QVERIFY(f.open(QIODevice::Append));QCOMPARE(f.write("changed"),qint64(7));}
        if(change=="mtime"){QFile f(sourcePath);QVERIFY(f.open(QIODevice::ReadWrite));QVERIFY(f.setFileTime(r.sourceVersion.modified.addSecs(10),QFileDevice::FileModificationTime));}
        if(change=="equal-length"||change=="restored-mtime"){
            QTest::qSleep(30);QVERIFY(write(sourcePath,green));
            if(change=="restored-mtime"){QFile f(sourcePath);QVERIFY(f.open(QIODevice::ReadWrite));QVERIFY(f.setFileTime(r.sourceVersion.modified,QFileDevice::FileModificationTime));}
            QCOMPARE(QFileInfo(sourcePath).size(),r.sourceVersion.size);
        }
        if(change=="deleted")QVERIFY(QFile::remove(sourcePath));
        if(change=="new-object"){
            QVERIFY(QFile::rename(sourcePath,d.filePath("held.mp3")));QVERIFY(QFile::copy(d.filePath("held.mp3"),sourcePath));
            QFile f(sourcePath);QVERIFY(f.open(QIODevice::ReadWrite));QVERIFY(f.setFileTime(r.sourceVersion.modified,QFileDevice::FileModificationTime));QCOMPARE(bytes(sourcePath),red);
        }
        if(change=="manual"){const auto p=d.filePath("manual.png");QVERIFY(write(p,png(Qt::green)));QVERIFY(l.updateArtwork(t.id,p));}
        if(change=="database"){QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec("CREATE TRIGGER reject_cover BEFORE UPDATE OF cover ON tracks BEGIN SELECT RAISE(ABORT,'P2 fixture'); END"));}
        if(change=="cache")QVERIFY(QDir().mkpath(target));
        // Snapshot after the deliberate mutation, before the attempted commit.
        const auto before=state(l,t.id,list);const auto names=QDir(l.absolute("Artwork")).entryList(QDir::AllEntries|QDir::NoDotAndDotDot);
        QVERIFY(captureDatabase(l,"before"));
#ifdef Q_OS_WIN
        HANDLE denied=INVALID_HANDLE_VALUE;
        if(change=="unreadable"){denied=CreateFileW(reinterpret_cast<LPCWSTR>(sourcePath.utf16()),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);QVERIFY(denied!=INVALID_HANDLE_VALUE);}
#endif
        const bool matches=l.artworkSnapshotMatches(r);const bool applied=l.applyArtworkRepair(r);
        diagnostic["after-artwork-commit"]=artwork_diagnostics::databaseState(l,"after-artwork-commit");
#ifdef Q_OS_WIN
        if(denied!=INVALID_HANDLE_VALUE)CloseHandle(denied);
#else
        if(change=="unreadable")QFAIL("Unreadable fixture requires Windows sharing semantics");
#endif
        const auto after=state(l,t.id,list);
        QVERIFY(captureDatabase(l,"after"));
        evidence(QString::fromLatin1(QTest::currentDataTag()),{{"change",change},{"expectedAccept",accept},{"snapshotMatches",matches},{"applied",applied},
            {"metadataEqual",r.sourceVersion==artworkFileVersion(sourcePath)},{"before",before},{"after",after},{"sourceOriginalSHA256",digest(red)},{"greenSHA256",digest(green)}});
        if(!accept){QCOMPARE(after,before);QCOMPARE(QDir(l.absolute("Artwork")).entryList(QDir::AllEntries|QDir::NoDotAndDotDot),names);}
        else{QCOMPARE(after["filesSHA256"].toObject()[t.file],before["filesSHA256"].toObject()[t.file]);QCOMPARE(after["filesSHA256"].toObject()[t.lyrics],before["filesSHA256"].toObject()[t.lyrics]);QCOMPARE(bytes(l.absolute(t.cover)),png(Qt::blue));QVERIFY(closeColor(QImage(l.absolute(l.track(t.id).cover)),Qt::red));QCOMPARE(after["playlistMembers"],before["playlistMembers"]);}
        QCOMPARE(applied,accept);
    }
    void cancelledLateCallback(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"cancel",Qt::red));QVERIFY(t.id);
        QVERIFY(l.saveLyricsPreservingText(t.id,"[00:01.00]Keep lyric\n"));const auto list=l.createPlaylist("Cancel");QVERIFY(l.addToPlaylist(list,{t.id}));const auto before=state(l,t.id,list);
        QSemaphore queued;ControlledCoordinator co(&l);co.libraryRoot=l.root();
        co.script=[&queued](ScriptedRepair &w){const auto r=Importer::repairArtwork(w.root,w.tracks[0]);queued.release();while(!w.token->load())QThread::msleep(2);w.repaired(r);w.finished();};
        QSignalSpy ready(&co,&ImportCoordinator::shutdownReady);QVERIFY(co.repairLegacyArtwork());QVERIFY(queued.tryAcquire(1,10000));co.shutdown();QTRY_COMPARE(ready.size(),1);
        const auto after=state(l,t.id,list);evidence("P2-09-cancel-late-callback",{{"before",before},{"after",after}});QCOMPARE(after,before);
    }
    void commitGuardExcludesConcurrentWriterAtSqlBoundary(){
#ifdef Q_OS_WIN
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"race",Qt::red));QVERIFY(t.id);
        QVERIFY(l.saveLyricsPreservingText(t.id,"[00:01.00]Race lyric\n"));const auto list=l.createPlaylist("Race");QVERIFY(l.addToPlaylist(list,{t.id}));t=l.track(t.id);
        const auto r=Importer::repairArtwork(l.root(),t);QCOMPARE(r.outcome,ArtworkOutcome::Recovered);
        struct Probe{QString path,target;bool called=false,writeBlocked=false,renameBlocked=false,deleteBlocked=false,cachePresent=false;DWORD writeError=0,renameError=0,deleteError=0;};
        Probe probe{l.absolute(t.file),l.absolute("Artwork/"+t.hash+"-v2-"+digest(r.png)+".png")};
        auto handle=import_fixture::database(l).driver()->handle();QVERIFY(handle.isValid());auto *native=*static_cast<sqlite3 **>(handle.data());
        auto callback=[](sqlite3_context *ctx,int,sqlite3_value **){
            auto &p=*static_cast<Probe *>(sqlite3_user_data(ctx));p.called=true;p.cachePresent=QFileInfo::exists(p.target);
            std::thread writer([&p]{
                const auto h=CreateFileW(reinterpret_cast<LPCWSTR>(p.path.utf16()),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
                p.writeBlocked=h==INVALID_HANDLE_VALUE;p.writeError=GetLastError();if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
                p.renameBlocked=!MoveFileW(reinterpret_cast<LPCWSTR>(p.path.utf16()),reinterpret_cast<LPCWSTR>((p.path+".moved").utf16()));p.renameError=GetLastError();
                p.deleteBlocked=!DeleteFileW(reinterpret_cast<LPCWSTR>(p.path.utf16()));p.deleteError=GetLastError();
            });writer.join();sqlite3_result_int(ctx,1);
        };
        QCOMPARE(sqlite3_create_function_v2(native,"p2_race",0,SQLITE_UTF8,&probe,callback,nullptr,nullptr,nullptr),SQLITE_OK);
        const auto detach=qScopeGuard([&]{sqlite3_create_function_v2(native,"p2_race",0,SQLITE_UTF8,nullptr,nullptr,nullptr,nullptr,nullptr);});
        QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec("CREATE TEMP TRIGGER p2_race_guard BEFORE UPDATE OF cover ON tracks BEGIN SELECT p2_race(); END"));
        const auto before=state(l,t.id,list);QVERIFY(l.applyArtworkRepair(r));const auto after=state(l,t.id,list);
        evidence("P2-race-SQL-boundary",{{"before",before},{"after",after},{"called",probe.called},{"cachePresent",probe.cachePresent},{"writeBlocked",probe.writeBlocked},{"renameBlocked",probe.renameBlocked},{"deleteBlocked",probe.deleteBlocked},{"writeError",int(probe.writeError)},{"renameError",int(probe.renameError)},{"deleteError",int(probe.deleteError)}});
        QVERIFY(probe.called&&probe.cachePresent&&probe.writeBlocked&&probe.renameBlocked&&probe.deleteBlocked);
        QCOMPARE(probe.writeError,DWORD(ERROR_SHARING_VIOLATION));QCOMPARE(probe.renameError,DWORD(ERROR_SHARING_VIOLATION));QCOMPARE(probe.deleteError,DWORD(ERROR_SHARING_VIOLATION));
        QCOMPARE(bytes(l.absolute(t.file)),bytes(d.filePath("race.mp3")));QCOMPARE(after["filesSHA256"].toObject()[t.lyrics],before["filesSHA256"].toObject()[t.lyrics]);QCOMPARE(bytes(l.absolute(t.cover)),png(Qt::blue));QCOMPARE(after["playlistMembers"],before["playlistMembers"]);
        QFile released(l.absolute(t.file));QVERIFY(released.open(QIODevice::ReadWrite)); // Lifetime ends after commit.
#endif
    }
    void streamingCostAndCancellation(){
        QTemporaryDir d;const auto path=d.filePath("large.bin");QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));
        const QByteArray block(1024*1024,'x');for(int i=0;i<64;++i)QCOMPARE(f.write(block),qint64(block.size()));f.close();
        ArtworkSourceGuard guard(path);QVERIFY(guard.isOpen());QElapsedTimer timer;timer.start();const auto first=guard.sha256();const auto ms=timer.elapsed();
        QCOMPARE(first.size(),32);std::atomic_bool cancel{true};QVERIFY(guard.sha256(&cancel).isEmpty());QCOMPARE(guard.sha256(),first);
        evidence("P2-streaming-cost",{{"bytesPerPass",64*1024*1024},{"bufferBytes",256*1024},{"milliseconds",double(ms)},{"sha256",QString::fromLatin1(first.toHex())},{"cancelledReadRejected",true}});
    }
};
QTEST_GUILESS_MAIN(ArtworkIntegrityTests)
#include "artwork-integrity-tests.moc"
