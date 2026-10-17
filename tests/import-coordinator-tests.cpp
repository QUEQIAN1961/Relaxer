#include "import-coordinator.h"
#include "import-fixture.h"
#include <QElapsedTimer>
#include <QSemaphore>
#include <QSignalSpy>
#include <QSqlDriver>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <qscopeguard.h>
#include <sqlite3.h>
#include <stdexcept>
#include <thread>

using namespace relaxer;
namespace {
ImportResult success(const QString &hash,const QString &title={}){
    ImportResult result;result.source=hash+".mid";result.track.hash=hash;result.track.title=title.isEmpty()?hash:title;
    result.track.file="Music/"+hash+".mid";result.track.coverOrigin="none";return result;
}
using Script=std::function<void(Importer &,const std::shared_ptr<std::atomic_bool> &)>;
struct Gate {
    QSemaphore reached,released;
    void wait(const std::shared_ptr<std::atomic_bool> &cancel){reached.release();while(!released.tryAcquire(1,10)&&!cancel->load()){} }
};
class ControlledImporter final : public Importer {
public:
    ControlledImporter(const QString &root,const QStringList &sources,Script run,std::shared_ptr<std::atomic_bool> token,std::shared_ptr<std::atomic_int> count)
        :Importer(root,sources),script(std::move(run)),cancel(std::move(token)),destroyed(std::move(count)){}
    ~ControlledImporter() override{++*destroyed;}
    void run() override{script(*this,cancel);}
    Script script;std::shared_ptr<std::atomic_bool> cancel;std::shared_ptr<std::atomic_int> destroyed;
};
class ControlledCoordinator final : public ImportCoordinator {
public:
    explicit ControlledCoordinator(Library *library):ImportCoordinator(library),library(library){}
    Script script;
    QPointer<Importer> lastWorker;
    std::shared_ptr<std::atomic_int> destroyed=std::make_shared<std::atomic_int>(0);
    bool refuse=false;
protected:
    Importer *createImporter(const ImportRequest &request,const std::shared_ptr<std::atomic_bool> &cancel) override{
        if(refuse)return nullptr;
        auto *worker=new ControlledImporter(library->root(),request.sources,script,cancel,destroyed);lastWorker=worker;return worker;
    }
private:Library *library;
};
ImportSummary summary(const QSignalSpy &spy,int row=0){return qvariant_cast<ImportSummary>(spy.at(row).first());}
}

class ImportCoordinatorTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QVERIFY(qobject_cast<QCoreApplication *>(QCoreApplication::instance()));QVERIFY(!toolPath("ffmpeg").isEmpty());}
    void rejectsInvalidBusyAndShuttingDownRequests(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);
        QSignalSpy rejected(&co,&ImportCoordinator::requestRejected),done(&co,&ImportCoordinator::finished),ready(&co,&ImportCoordinator::shutdownReady);
        QCOMPARE(co.start({}),quint64(0));QCOMPARE(rejected.size(),1);
        Gate gate;auto release=qScopeGuard([&]{gate.released.release();});
        co.script=[&](Importer &w,const auto &token){w.scanned(0,0,true);gate.wait(token);w.finished(token->load());};
        const auto id=co.start({{"fixture.mid"},42,1});QVERIFY(id);QVERIFY(gate.reached.tryAcquire(1,5000));
        QCOMPARE(co.start({{"another.mid"}}),quint64(0));QCOMPARE(co.activeTaskId(),id);
        co.shutdown();QVERIFY(co.isShuttingDown());QVERIFY(!co.cancel(id+1));
        QTRY_COMPARE(done.size(),1);QCOMPARE(ready.size(),1);QVERIFY(co.isIdle());QVERIFY(summary(done).cancelled);
        co.shutdown();QCOMPARE(ready.size(),1);QCOMPARE(co.start({{"new.mid"}}),quint64(0));
    }
    void rejectsForeignCallingThreadAndLibraryAffinity(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ImportCoordinator co(&library);
        quint64 result=999;std::thread foreign([&]{result=co.start({{"test.mid"}});});foreign.join();QCOMPARE(result,quint64(0));QVERIFY(co.isIdle());
        QThread other;ImportCoordinator wrong(&library);wrong.moveToThread(&other); // No SQL connection is moved.
        QCOMPARE(wrong.start({{"test.mid"}}),quint64(0));
        // Move the service back from its own thread before stack destruction.
        other.start();QMetaObject::invokeMethod(&wrong,[&]{result=wrong.start({{"test.mid"}});wrong.moveToThread(QCoreApplication::instance()->thread());},Qt::BlockingQueuedConnection);other.quit();QVERIFY(other.wait(5000));QCOMPARE(result,quint64(0));
    }
    void failedFactoryLeavesNoTask(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);co.refuse=true;
        QCOMPARE(co.start({{"test.mid"}}),quint64(0));QVERIFY(co.isIdle());QVERIFY(!co.lastError().isEmpty());
    }
    void realFilesDuplicatesPlaylistsAndDatabaseReopen(){
        QTemporaryDir directory;const auto root=directory.filePath("data"),a=directory.filePath("one.mid"),b=directory.filePath("copy.mid"),c=directory.filePath("second.mid");
        QVERIFY(import_fixture::midi(a,1));QVERIFY(QFile::copy(a,b));QVERIFY(import_fixture::midi(c,2));
        qint64 first=0,second=0,existingId=0;QStringList schema;
        {
            Library library(root);QVERIFY(library.open());schema=import_fixture::schema(library);first=library.createPlaylist("first");second=library.createPlaylist("second");
            ImportCoordinator co(&library);QSignalSpy done(&co,&ImportCoordinator::finished),committed(&co,&ImportCoordinator::trackCommitted);
            bool correctThread=true;connect(&co,&ImportCoordinator::trackCommitted,this,[&]{correctThread&=QThread::currentThread()==library.thread();});
            const auto id=co.start({{a,b,c,a,directory.filePath("missing.wav")},first,1});QVERIFY(id);QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);
            auto report=summary(done);QCOMPARE(report.taskId,id);QCOMPARE(report.sourceCount,5);QCOMPARE(report.totalFiles,3);QCOMPARE(report.ignoredPaths,2);
            QCOMPARE(report.added,2);QCOMPARE(report.duplicates,1);QCOMPARE(report.failed,0);QCOMPARE(report.processed,3);QCOMPARE(report.skipped,2);QVERIFY(report.scanComplete);QVERIFY(!report.cancelled);
            QCOMPARE(committed.size(),3);QVERIFY(correctThread);QCOMPARE(library.tracks(first).size(),2);QCOMPARE(library.tracks(second).size(),0);
            existingId=library.tracks(first).first().id;QVERIFY(library.favorite(existingId,true));
            const auto next=co.start({{a},second,1});QVERIFY(next>id);QTRY_COMPARE_WITH_TIMEOUT(done.size(),2,20000);
            QCOMPARE(summary(done,1).duplicates,1);QCOMPARE(summary(done,1).added,0);QCOMPARE(library.tracks(second).size(),1);QCOMPARE(import_fixture::schema(library),schema);
        }
        Library reopened(root);QVERIFY(reopened.open());QCOMPARE(reopened.tracks().size(),2);QCOMPARE(reopened.tracks(first).size(),2);QCOMPARE(reopened.tracks(second).size(),1);QVERIFY(reopened.track(existingId).favorite);
        for(const auto &track:reopened.tracks())QVERIFY(QFileInfo::exists(reopened.absolute(track.file)));QCOMPARE(import_fixture::schema(reopened),schema);
    }
    void realBadMediaIsPartialFailureAndCoverRemainsOptional(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());
        const auto good=directory.filePath("good.wav"),bad=directory.filePath("bad.mp3");QVERIFY(import_fixture::wave(good));
        QFile corrupt(bad);QVERIFY(corrupt.open(QIODevice::WriteOnly));corrupt.write("synthetic invalid media");corrupt.close();
        QFile cover(directory.filePath("good.png"));QVERIFY(cover.open(QIODevice::WriteOnly));cover.write("bad cover");cover.close();
        ImportCoordinator co(&library);QSignalSpy done(&co,&ImportCoordinator::finished);QVERIFY(co.start({{good,bad},0,1}));QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);
        const auto report=summary(done);QCOMPARE(report.added,1);QCOMPARE(report.failed,1);QVERIFY(report.partialSuccess);QCOMPARE(library.tracks().size(),1);QVERIFY(QFileInfo::exists(good));
    }
    void sqlFailureIsLocalAndActualWritesStayOnLibraryThread(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());
        auto db=import_fixture::database(library);auto handle=db.driver()->handle();QVERIFY(handle.isValid());auto *native=*static_cast<sqlite3 **>(handle.data());
        struct Probe {QThread *owner;int calls=0;bool correct=true;} probe{library.thread()};
        const auto callback=+[](sqlite3_context *context,int,sqlite3_value **){auto &p=*static_cast<Probe *>(sqlite3_user_data(context));++p.calls;p.correct&=QThread::currentThread()==p.owner;sqlite3_result_int(context,1);};
        QCOMPARE(sqlite3_create_function_v2(native,"b6_owner_thread",0,SQLITE_UTF8,&probe,callback,nullptr,nullptr,nullptr),SQLITE_OK);
        QSqlQuery query(db);QVERIFY(query.exec("CREATE TRIGGER b6_owner BEFORE INSERT ON tracks BEGIN SELECT b6_owner_thread(); END"));
        QVERIFY(query.exec("CREATE TRIGGER b6_reject BEFORE INSERT ON tracks WHEN NEW.title='reject' BEGIN SELECT RAISE(ABORT,'B6 injected insert failure'); END"));
        ControlledCoordinator co(&library);co.script=[](Importer &w,const auto &){w.scanned(3,0,true);w.imported(success("first"));w.imported(success("bad","reject"));w.imported(success("last"));w.finished(false);};
        QSignalSpy done(&co,&ImportCoordinator::finished);QVERIFY(co.start({{"fixture"}}));QTRY_COMPARE(done.size(),1);const auto report=summary(done);
        QCOMPARE(report.added,2);QCOMPARE(report.failed,1);QVERIFY(report.partialSuccess);QCOMPARE(library.tracks().size(),2);QVERIFY(probe.calls>=2);QVERIFY(probe.correct);QVERIFY(report.errors.join('\n').contains("B6 injected"));
    }
    void targetPlaylistIsFixedAndDeletedTargetDoesNotLoseSong(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());const auto target=library.createPlaylist("delete during import"),other=library.createPlaylist("other");
        ControlledCoordinator co(&library);Gate gate;auto release=qScopeGuard([&]{gate.released.release();});
        co.script=[&](Importer &w,const auto &token){w.scanned(1,0,true);gate.wait(token);w.imported(success("fixed-target"));w.finished(false);};
        QSignalSpy done(&co,&ImportCoordinator::finished);QVERIFY(co.start({{"fixture"},target}));QVERIFY(gate.reached.tryAcquire(1,5000));QVERIFY(library.deletePlaylist(target));gate.released.release();
        QTRY_COMPARE(done.size(),1);const auto report=summary(done);QCOMPARE(report.targetPlaylist,target);QCOMPARE(report.added,1);QCOMPARE(report.failed,0);QCOMPARE(report.playlistFailures,1);QVERIFY(report.partialSuccess);QCOMPARE(library.tracks().size(),1);QCOMPARE(library.tracks(other).size(),0);
    }
    void cancelBeforeWorkDoesNotStartFilesAndIsIdempotent(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);Gate gate;
        auto release=qScopeGuard([&]{gate.released.release();});co.script=[&](Importer &w,const auto &token){w.scanned(3,0,true);gate.wait(token);if(!token->load())w.imported(success("unexpected"));w.finished(token->load());};
        QSignalSpy done(&co,&ImportCoordinator::finished);const auto id=co.start({{"fixture"}});QVERIFY(id);QVERIFY(gate.reached.tryAcquire(1,5000));
        QVERIFY(co.cancel(id));QVERIFY(co.cancel(id));QTRY_COMPARE(done.size(),1);const auto report=summary(done);QVERIFY(report.cancelled);QCOMPARE(report.processed,0);QCOMPARE(report.unstarted,3);QCOMPARE(report.failed,0);QVERIFY(library.tracks().isEmpty());QVERIFY(!co.cancel(id));
    }
    void cancellationKeepsAlreadyQueuedInFlightSuccess(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);Gate gate;auto release=qScopeGuard([&]{gate.released.release();});
        co.script=[&](Importer &w,const auto &token){w.scanned(3,0,true);w.imported(success("before-cancel"));gate.wait(token);w.imported(success("in-flight-success"));ImportResult failed;failed.error="已取消";w.imported(failed);w.finished(true);};
        QSignalSpy done(&co,&ImportCoordinator::finished);const auto id=co.start({{"fixture"}});QVERIFY(gate.reached.tryAcquire(1,5000));QVERIFY(co.cancel(id));QTRY_COMPARE(done.size(),1);
        const auto report=summary(done);QVERIFY(report.cancelled);QCOMPARE(report.added,2);QCOMPARE(report.failed,1);QCOMPARE(report.processed,3);QVERIFY(report.partialSuccess);QCOMPARE(library.tracks().size(),2);
    }
    void completionWinsLateCancelAndRejectsLateAndRepeatedCallbacks(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);Gate gate;auto release=qScopeGuard([&]{gate.released.release();});
        co.script=[&](Importer &w,const auto &token){w.scanned(1,0,true);w.imported(success("valid"));w.finished(false);w.imported(success("late"));w.finished(true);gate.wait(token);};
        QSignalSpy done(&co,&ImportCoordinator::finished);const auto id=co.start({{"fixture"}});QVERIFY(gate.reached.tryAcquire(1,5000));QVERIFY(!co.cancel(id));
        QCoreApplication::processEvents();QCOMPARE(done.size(),0);gate.released.release();QTRY_COMPARE(done.size(),1);QVERIFY(!summary(done).cancelled);QCOMPARE(summary(done).added,1);QCOMPARE(library.tracks().size(),1);
        co.script=[](Importer &w,const auto &){w.scanned(1,0,true);w.imported(success("new-task"));w.finished(false);};
        const auto second=co.start({{"second"}});QVERIFY(second>id);QVERIFY(!co.cancel(id));QTRY_COMPARE(done.size(),2);QCOMPARE(summary(done,1).taskId,second);QCOMPARE(summary(done,1).added,1);QCOMPARE(library.tracks().size(),2);
        for(const auto &track:library.tracks())QVERIFY(track.hash!="late");
    }
    void cancellationWinsBeforeWorkerReportsNormalCompletion(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);Gate gate;auto release=qScopeGuard([&]{gate.released.release();});
        co.script=[&](Importer &w,const auto &token){gate.wait(token);w.finished(false);};QSignalSpy done(&co,&ImportCoordinator::finished);
        const auto id=co.start({{"fixture"}});QVERIFY(gate.reached.tryAcquire(1,5000));QVERIFY(co.cancel(id));QTRY_COMPARE(done.size(),1);QVERIFY(summary(done).cancelled);
    }
    void destructionCancelsWithoutBlockingAndReleasesWorkerAndThread(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());auto co=std::make_unique<ControlledCoordinator>(&library);Gate gate;
        auto release=qScopeGuard([&]{gate.released.release();});co->script=[&](Importer &w,const auto &token){gate.wait(token);w.imported(success("must-not-write-after-destruction"));w.finished(true);};
        int callbacks=0;connect(co.get(),&ImportCoordinator::finished,this,[&]{++callbacks;});QVERIFY(co->start({{"fixture"}}));QVERIFY(gate.reached.tryAcquire(1,5000));
        auto worker=co->lastWorker;QPointer<QThread> thread=worker->thread();auto destroyed=co->destroyed;
        QElapsedTimer timer;timer.start();co.reset();QVERIFY(timer.elapsed()<1000);
        QTRY_COMPARE(destroyed->load(),1);QTRY_VERIFY(worker.isNull());QTRY_VERIFY(thread.isNull());QCOMPARE(callbacks,0);QVERIFY(library.tracks().isEmpty());
    }
    void backendReturnOrExceptionCannotStrandThread_data(){QTest::addColumn<bool>("throws");QTest::newRow("missing-finished")<<false;QTest::newRow("exception")<<true;}
    void backendReturnOrExceptionCannotStrandThread(){
        QFETCH(bool,throws);QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);
        co.script=[throws](Importer &w,const auto &){w.scanned(1,0,true);w.imported(success("completed-before-return"));if(throws)throw std::runtime_error("controlled exception");};
        QSignalSpy done(&co,&ImportCoordinator::finished);QVERIFY(co.start({{"fixture"}}));auto worker=co.lastWorker;QPointer<QThread> thread=worker->thread();QTRY_COMPARE(done.size(),1);
        QCOMPARE(summary(done).added,1);QCOMPARE(!summary(done).fatalError.isEmpty(),throws);QTRY_VERIFY(worker.isNull());QTRY_VERIFY(thread.isNull());QVERIFY(co.isIdle());
    }
    void progressFloodIsCoalescedAndMonotonic(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());ControlledCoordinator co(&library);
        co.script=[](Importer &w,const auto &){w.scanned(1,0,true);for(int i=0;i<100000;++i)w.progress(i%2,1,"progress");w.imported(success("progress"));w.progress(1,1,"finished");w.finished(false);};
        QSignalSpy progress(&co,&ImportCoordinator::progress),done(&co,&ImportCoordinator::finished);const auto id=co.start({{"fixture"}});QTRY_COMPARE(done.size(),1);
        QVERIFY(!progress.isEmpty());QVERIFY2(progress.size()<100,"Progress signals were not coalesced");int previous=0;
        for(const auto &event:progress){QCOMPARE(event.at(0).toULongLong(),id);QVERIFY(event.at(1).toInt()>=previous);previous=event.at(1).toInt();}QCOMPARE(previous,1);
    }
    void cancelStopsStalledProbeProcess(){
        QTemporaryDir directory;const auto tools=directory.filePath("tools"),root=directory.filePath("data"),source=directory.filePath("source.wav"),marker=directory.filePath("probe-ready");
        QVERIFY(QDir().mkpath(tools));QFile file(source);QVERIFY(file.open(QIODevice::WriteOnly));file.write("synthetic probe fixture");file.close();
#ifdef Q_OS_WIN
        const auto fixture=QCoreApplication::applicationDirPath()+"/relaxer_import_process_fixture.exe",probe=tools+"/ffprobe.exe";
#else
        const auto fixture=QCoreApplication::applicationDirPath()+"/relaxer_import_process_fixture",probe=tools+"/ffprobe";
#endif
        QVERIFY(QFile::copy(fixture,probe));
        const auto oldTools=qgetenv("RELAXER_FFMPEG_DIR"),oldMarker=qgetenv("RELAXER_IMPORT_PROCESS_MARKER");
        auto restore=qScopeGuard([&]{if(oldTools.isNull())qunsetenv("RELAXER_FFMPEG_DIR");else qputenv("RELAXER_FFMPEG_DIR",oldTools);if(oldMarker.isNull())qunsetenv("RELAXER_IMPORT_PROCESS_MARKER");else qputenv("RELAXER_IMPORT_PROCESS_MARKER",oldMarker);});
        qputenv("RELAXER_FFMPEG_DIR",tools.toUtf8());qputenv("RELAXER_IMPORT_PROCESS_MARKER",QFile::encodeName(marker));
        QCOMPARE(QFileInfo(toolPath("ffprobe")).canonicalFilePath(),QFileInfo(probe).canonicalFilePath());
        std::atomic_bool cancel{false},complete{false};ImportResult result;
        std::thread worker([&]{result=Importer::importOne(root,source,&cancel);complete.store(true);});
        auto join=qScopeGuard([&]{cancel.store(true);if(worker.joinable())worker.join();});
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker),5000);QElapsedTimer elapsed;elapsed.start();cancel.store(true);
        QTRY_VERIFY_WITH_TIMEOUT(complete.load(),3000);worker.join();QVERIFY(elapsed.elapsed()<3000);QCOMPARE(result.error,QStringLiteral("已取消"));QVERIFY(QFileInfo::exists(source));
    }
    void callbackMayDestroyCoordinator_data(){QTest::addColumn<bool>("duringProgress");QTest::newRow("final-progress")<<true;QTest::newRow("final-result")<<false;}
    void callbackMayDestroyCoordinator(){
        QFETCH(bool,duringProgress);QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());auto co=std::make_unique<ControlledCoordinator>(&library);
        co->script=[](Importer &w,const auto &){w.scanned(1,0,true);w.imported(success("callback-destruction"));w.progress(1,1,"last");w.finished(false);};
        auto destroyed=co->destroyed;int callbacks=0;
        if(duringProgress)connect(co.get(),&ImportCoordinator::progress,this,[&]{++callbacks;co.reset();});
        else connect(co.get(),&ImportCoordinator::finished,this,[&]{++callbacks;co.reset();});
        QVERIFY(co->start({{"fixture"}}));QTRY_VERIFY(!co);QTRY_COMPARE(destroyed->load(),1);QCOMPARE(callbacks,1);
    }
    void libraryCanDisappearWhileWorkerIsRunning(){
        QTemporaryDir directory;auto library=std::make_unique<Library>(directory.filePath("data"));QVERIFY(library->open());ControlledCoordinator co(library.get());Gate gate;auto release=qScopeGuard([&]{gate.released.release();});
        co.script=[&](Importer &w,const auto &token){w.scanned(1,0,true);gate.wait(token);w.imported(success("no-library"));w.finished(false);};QSignalSpy done(&co,&ImportCoordinator::finished);
        QVERIFY(co.start({{"fixture"}}));QVERIFY(gate.reached.tryAcquire(1,5000));library.reset();gate.released.release();QTRY_COMPARE(done.size(),1);QCOMPARE(summary(done).failed,1);QVERIFY(co.isIdle());
    }
    void shutdownWaitsForImportAndArtworkAndPreventsNewTasks(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());
        const auto path=directory.filePath("legacy.wav");QVERIFY(import_fixture::wave(path));auto result=Importer::importOne(library.root(),path);QVERIFY(result.error.isEmpty());
        result.track.cover="Artwork/"+result.track.hash+".png";result.track.coverOrigin.clear();QVERIFY(library.insert(result.track));
        ControlledCoordinator co(&library);Gate gate;auto release=qScopeGuard([&]{gate.released.release();});co.script=[&](Importer &w,const auto &token){gate.wait(token);w.finished(token->load());};
        QSignalSpy done(&co,&ImportCoordinator::finished),art(&co,&ImportCoordinator::artworkFinished),ready(&co,&ImportCoordinator::shutdownReady);
        QVERIFY(co.repairLegacyArtwork());QVERIFY(co.start({{"fixture"}}));QVERIFY(gate.reached.tryAcquire(1,5000));co.shutdown();
        QCOMPARE(co.start({{"other"}}),quint64(0));QTRY_COMPARE_WITH_TIMEOUT(ready.size(),1,20000);QCOMPARE(done.size(),1);QCOMPARE(art.size(),1);QVERIFY(co.isIdle());
    }
};
QTEST_GUILESS_MAIN(ImportCoordinatorTests)
#include "import-coordinator-tests.moc"
