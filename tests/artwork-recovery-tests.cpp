#include <QtTest>
#include "artwork-recovery-fixture.h"
#include "c4c5-ui-fixture.h"
#include "ui-bridge/ui-images.h"
#include "widgets.h"
#include <QScopeGuard>
#include <QPixmapCache>
#include <QUrlQuery>
using namespace relaxer;
using namespace artwork_fixture;
namespace {
ArtworkSummary summary(const QSignalSpy &spy){return qvariant_cast<ArtworkSummary>(spy.last().first());}
QStringList cacheFiles(const Library &l){return QDir(l.absolute("Artwork")).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot);}
QImage qmlImage(const QString &path,const QString &seed="same") {
    UiImages provider;QUrl url("image://relaxer/artwork");QUrlQuery q;q.addQueryItem("file",QUrl::fromLocalFile(path).toString());q.addQueryItem("seed",seed);url.setQuery(q);
    return provider.requestImage(url.toString().mid(QString("image://relaxer/").size()),nullptr,QSize(96,96));
}
}
class ArtworkRecoveryTests final:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QCOMPARE(QGuiApplication::platformName(),QString("offscreen"));QVERIFY(!toolPath("ffmpeg").isEmpty());QVERIFY(!toolPath("ffprobe").isEmpty());}
    void extractionStagedCheckedAndIdempotent(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto source=embedded(d.path(),"red",Qt::red);QVERIFY(!source.isEmpty());
        auto t=legacy(l,source);QVERIFY(t.id);const auto old=bytes(l.absolute(t.cover)),audio=bytes(l.absolute(t.file));const auto before=cacheFiles(l);
        const auto result=Importer::repairArtwork(l.root(),t);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(result.outcome,ArtworkOutcome::Recovered);
        QCOMPARE(cacheFiles(l),before);QVERIFY(closeColor(decodeRecoveredArtwork(result.png),Qt::red));QVERIFY(l.applyArtworkRepair(result));
        const auto updated=l.track(t.id);QCOMPARE(updated.coverOrigin,QString("embedded"));QVERIFY(updated.cover.contains("-v2-"));QVERIFY(closeColor(QImage(l.absolute(updated.cover)),Qt::red));
        QCOMPARE(bytes(l.absolute(t.cover)),old);QCOMPARE(bytes(l.absolute(t.file)),audio);const auto after=cacheFiles(l);QVERIFY(!l.applyArtworkRepair(result));QCOMPARE(cacheFiles(l),after);
        ImportCoordinator co(&l);QVERIFY(!co.repairLegacyArtwork()); // Current valid cache is not rescanned.
    }
    void staleIdentity_data(){QTest::addColumn<QString>("change");for(const auto *s:{"deleted","manual","hash","database-file","source-bytes","source-replaced","old-cache"})QTest::newRow(s)<<QString(s);}
    void staleIdentity(){
        QFETCH(QString,change);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"guard",Qt::red));QVERIFY(t.id);
        const auto r=Importer::repairArtwork(l.root(),t);QCOMPARE(r.outcome,ArtworkOutcome::Recovered);
        if(change=="deleted")QVERIFY(l.remove({t.id}));
        else if(change=="manual"){const auto p=d.filePath("manual.png");QVERIFY(write(p,png(Qt::green)));QVERIFY(l.updateArtwork(t.id,p));}
        else if(change=="source-bytes"){QFile f(l.absolute(t.file));QVERIFY(f.open(QIODevice::Append));QCOMPARE(f.write("changed"),qint64(7));f.close();}
        else if(change=="source-replaced") {QTest::qWait(3);const auto old=l.absolute(t.file)+".moved";QVERIFY(QFile::rename(l.absolute(t.file),old));QVERIFY(QFile::copy(old,l.absolute(t.file)));QFile f(l.absolute(t.file));QVERIFY(f.open(QIODevice::ReadWrite));QVERIFY(f.setFileTime(r.sourceVersion.modified,QFileDevice::FileModificationTime));}
        else if(change=="old-cache")QVERIFY(write(l.absolute(t.cover),png(Qt::yellow)));
        else {QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec(QString("UPDATE tracks SET %1='%2' WHERE id=%3").arg(change=="hash"?"hash":"file",change=="hash"?"changed":"Music/else.mp3").arg(t.id)));}
        const auto files=cacheFiles(l);QVERIFY(!l.applyArtworkRepair(r));QCOMPARE(cacheFiles(l),files);
        if(change=="manual")QVERIFY(closeColor(QImage(l.absolute(l.track(t.id).cover)),Qt::green));
    }
    void noSourceRetainsValidOldCoverAndPersistsReceipt(){
        QTemporaryDir d;const auto root=d.filePath("data");qint64 id=0;QString cover;QByteArray old;
        {Library l(root);QVERIFY(l.open());const auto source=d.filePath("plain.wav");QVERIFY(import_fixture::wave(source));auto t=legacy(l,source);QVERIFY(t.id);id=t.id;cover=t.cover;old=bytes(l.absolute(t.cover));
         // Even a same-basename image beside the managed audio is not a recovery source.
         QVERIFY(write(QFileInfo(l.absolute(t.file)).dir().filePath(QFileInfo(t.file).completeBaseName()+".png"),png(Qt::green)));
         ImportCoordinator co(&l);QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE(done.size(),1);
         const auto s=summary(done);QCOMPARE(s.noSource,1);QCOMPARE(s.retained,1);QCOMPARE(s.recovered,0);QVERIFY(l.track(id).coverOrigin.isEmpty());QCOMPARE(bytes(l.absolute(cover)),old);QVERIFY(!co.repairLegacyArtwork());}
        {Library l(root);QVERIFY(l.open());ImportCoordinator co(&l);QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(!co.repairLegacyArtwork());QVERIFY(co.repairLegacyArtwork(true));QTRY_COMPARE(done.size(),1);QCOMPARE(summary(done).retained,1);QCOMPARE(bytes(l.absolute(cover)),old);}
    }
    void invalidImages_data(){
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("missing")<<QByteArray();QTest::newRow("empty")<<QByteArray(0,'x');QTest::newRow("fake-image")<<QByteArray("pretend.png not an image");
        QTest::newRow("truncated")<<png(Qt::red).left(15);QTest::newRow("too-many-bytes")<<QByteArray(maximumArtworkBytes+1,'x');
        QImage huge(801,1,QImage::Format_RGB32);huge.fill(Qt::red);QByteArray b;QBuffer out(&b);out.open(QIODevice::WriteOnly);QVERIFY(huge.save(&out,"PNG"));QTest::newRow("not-normalized-png")<<b;
    }
    void invalidImages(){
        QFETCH(QByteArray,data);QVERIFY(decodeRecoveredArtwork(data).isNull());
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"payload",Qt::red));QVERIFY(t.id);
        auto r=Importer::repairArtwork(l.root(),t);r.png=data;const auto old=bytes(l.absolute(t.cover));const auto files=cacheFiles(l);
        QVERIFY(!l.applyArtworkRepair(r));QCOMPARE(bytes(l.absolute(t.cover)),old);QCOMPARE(cacheFiles(l),files);
    }
    void invalidCacheFallsBackInBothFrontends_data(){QTest::addColumn<QString>("kind");for(const auto *s:{"missing","empty","non-image","truncated","oversized-file","oversized-pixels"})QTest::newRow(s)<<QString(s);}
    void invalidCacheFallsBackInBothFrontends(){
        QFETCH(QString,kind);QTemporaryDir d;const auto path=d.filePath("bad.png");
        if(kind=="empty")QVERIFY(write(path,QByteArray("")));
        else if(kind=="non-image")QVERIFY(write(path,"not an image"));
        else if(kind=="truncated")QVERIFY(write(path,png(Qt::red).left(14)));
        else if(kind=="oversized-file")QVERIFY(write(path,png(Qt::red)+QByteArray(maximumArtworkBytes,'x')));
        else if(kind=="oversized-pixels")QVERIFY(write(path,"P6\n20000 20000\n255\n"));
        QVERIFY(readArtworkImage(path,QSize(96,96)).isNull());const auto qml=qmlImage(path),widgets=coverPixmap(path,"same",96).toImage();
        QVERIFY(!qml.isNull());QVERIFY(!widgets.isNull());QVERIFY(!closeColor(qml,Qt::red));QVERIFY(!closeColor(widgets,Qt::red));
    }
    void corruptEmbeddedPictureIsRejected(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto source=d.filePath("broken.mp3");QVERIFY(ffmpeg({"-f","lavfi","-i","sine=duration=0.2","-c:a","libmp3lame",source}));
        const auto audio=bytes(source);QByteArray apic;apic.append(char(0));apic+="image/png";apic.append(char(0));apic.append(char(3));apic.append(char(0));apic+="NOT_PNG_BROKEN";
        auto be=[](quint32 n){QByteArray b;for(int i=3;i>=0;--i)b.append(char((n>>(8*i))&255));return b;};
        const QByteArray frame=QByteArray("APIC")+be(apic.size())+QByteArray(2,'\0')+apic;quint32 n=frame.size();QByteArray size;
        for(int i=3;i>=0;--i)size.append(char((n>>(7*i))&127));
        QVERIFY(write(source,QByteArray("ID3\3\0\0",6)+size+frame+audio));auto t=legacy(l,source);QVERIFY(t.id);
        const auto r=Importer::repairArtwork(l.root(),t);QCOMPARE(r.outcome,ArtworkOutcome::InvalidImage);QVERIFY(!r.error.isEmpty());QVERIFY(r.png.isEmpty());QVERIFY(!l.applyArtworkRepair(r));QVERIFY(closeColor(QImage(l.absolute(t.cover)),Qt::blue));
    }
    void sameNameDifferentTrackNeverSharesArtwork(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=legacy(l,embedded(d.path(),"a",Qt::red,440));auto b=legacy(l,embedded(d.path(),"b",Qt::green,660));QVERIFY(a.id&&b.id);QCOMPARE(a.title,b.title);QCOMPARE(a.album,b.album);
        const auto list=l.createPlaylist("same album");QVERIFY(l.addToPlaylist(list,{a.id,b.id}));const auto schema=import_fixture::schema(l);
        ImportCoordinator co(&l);QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);QCOMPARE(summary(done).recovered,2);
        a=l.track(a.id);b=l.track(b.id);QVERIFY(a.cover!=b.cover);QVERIFY(closeColor(QImage(l.absolute(a.cover)),Qt::red));QVERIFY(closeColor(QImage(l.absolute(b.cover)),Qt::green));QCOMPARE(l.tracks(list).size(),2);QCOMPARE(import_fixture::schema(l),schema);
    }
    void writeFailures_data(){QTest::addColumn<QString>("kind");for(const auto *s:{"cache-collision","cache-directory","sql-rejection"})QTest::newRow(s)<<QString(s);}
    void writeFailures(){
        QFETCH(QString,kind);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"failure",Qt::red));QVERIFY(t.id);const auto r=Importer::repairArtwork(l.root(),t);
        const auto target=l.absolute("Artwork/"+t.hash+"-v2-"+QString::fromLatin1(QCryptographicHash::hash(r.png,QCryptographicHash::Sha256).toHex())+".png");
        if(kind=="cache-collision")QVERIFY(write(target,png(Qt::yellow)));
        else if(kind=="cache-directory")QVERIFY(QDir().mkpath(target));
        else {QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec("CREATE TRIGGER reject_art BEFORE UPDATE OF cover ON tracks BEGIN SELECT RAISE(ABORT,'fixture cache SQL rejection'); END"));}
        const auto old=bytes(l.absolute(t.cover));const auto files=cacheFiles(l);QVERIFY(!l.applyArtworkRepair(r));QVERIFY(!l.lastError().isEmpty());QCOMPARE(bytes(l.absolute(t.cover)),old);QCOMPARE(l.track(t.id).cover,t.cover);QCOMPARE(cacheFiles(l),files);
    }
    void cacheOpenFailureKeepsReference(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"blocked",Qt::red),QByteArray());QVERIFY(t.id);const auto r=Importer::repairArtwork(l.root(),t);
        QVERIFY(QDir().rename(l.absolute("Artwork"),l.absolute("Artwork-kept")));QVERIFY(write(l.absolute("Artwork"),"blocked parent"));QVERIFY(l.artworkSnapshotMatches(r));
        QVERIFY(!l.applyArtworkRepair(r));QCOMPARE(l.track(t.id).cover,t.cover);QCOMPARE(bytes(l.absolute("Artwork")),QByteArray("blocked parent"));
    }
    void changedSourceRetriesAndTransientErrorsCoolDown(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto wav=d.filePath("plain.wav");QVERIFY(import_fixture::wave(wav));auto t=legacy(l,wav,QByteArray());QVERIFY(t.id);
        const auto held=l.absolute(t.file)+".held";QVERIFY(QFile::rename(l.absolute(t.file),held));
        ImportCoordinator co(&l);QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE(done.size(),1);QCOMPARE(summary(done).failed,1);QVERIFY(!co.repairLegacyArtwork());
        const auto key=QString("artwork.recovery.v1.%1").arg(t.id);QVERIFY(l.setting(key).toObject()["retryAt"].toDouble()>QDateTime::currentMSecsSinceEpoch());
        QVERIFY(QFile::rename(held,l.absolute(t.file)));QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE(done.size(),2);QCOMPARE(summary(done).noSource,1);QCOMPARE(summary(done).placeholder,1);QVERIFY(!co.repairLegacyArtwork());
    }
    void selectionManualAndCurrentAreNotRecoveryTargets(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto source=embedded(d.path(),"normal",Qt::red);auto imported=Importer::importOne(l.root(),source);QVERIFY(imported.error.isEmpty());const auto id=l.insert(imported.track);QVERIFY(id);
        ImportCoordinator co(&l);QVERIFY(!co.repairLegacyArtwork());const auto manual=d.filePath("manual.png");QVERIFY(write(manual,png(Qt::green)));QVERIFY(l.updateArtwork(id,manual));QVERIFY(!co.repairLegacyArtwork());QVERIFY(QFile::remove(l.absolute(l.track(id).cover)));QVERIFY(!co.repairLegacyArtwork());
        QVERIFY(!isLegacyArtwork(l.track(id))); // Missing manual file remains user-controlled, uses placeholder.
    }
    void stalePlanFrozenBeforeWorkerBegins(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"late",Qt::red));QVERIFY(t.id);const auto files=cacheFiles(l);
        ControlledCoordinator co(&l);co.libraryRoot=l.root();co.script=[](ScriptedRepair &w){QFile f(QDir(w.root).filePath(w.tracks[0].file));if(!f.open(QIODevice::Append))return;f.write("changed after plan");f.close();w.repaired(Importer::repairArtwork(w.root,w.tracks[0]));w.finished();};
        QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE(done.size(),1);QCOMPARE(summary(done).stale,1);QCOMPARE(summary(done).recovered,0);QCOMPARE(cacheFiles(l),files);QCOMPARE(l.track(t.id).cover,t.cover);
    }
    void partialBatchIsBoundedAndSuccessfulTargetsSurvive(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto success=legacy(l,embedded(d.path(),"one",Qt::red));QVERIFY(success.id);
        const auto wav=d.filePath("plain.wav");QVERIFY(import_fixture::wave(wav));
        for(int i=0;i<22;++i){Track t;t.hash=QString("invalid-%1").arg(i);t.file="Music/"+t.hash+".wav";t.cover="Artwork/"+t.hash+".png";QVERIFY(QFile::copy(wav,l.absolute(t.file)));QVERIFY(l.insert(t));}
        ControlledCoordinator co(&l);co.libraryRoot=l.root();co.script=[](ScriptedRepair &w){for(const auto &t:w.tracks){auto r=Importer::repairArtwork(w.root,t);if(t.hash.startsWith("invalid")){r.outcome=ArtworkOutcome::InvalidImage;r.error="synthetic invalid embedded image";}w.repaired(r);}w.finished();};
        QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);const auto s=summary(done);QCOMPARE(s.total,23);QCOMPARE(s.processed,23);QCOMPARE(s.recovered,1);QCOMPARE(s.invalid,22);QCOMPARE(s.errors.size(),8);QCOMPARE(s.placeholder,22);QVERIFY(closeColor(QImage(l.absolute(l.track(success.id).cover)),Qt::red));
    }
    void producerWaitsForOwnerAndCancellationDropsQueuedResult(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"pending",Qt::red));QVERIFY(t.id);QVERIFY(legacy(l,embedded(d.path(),"pending-second",Qt::green,660)).id);const auto files=cacheFiles(l);
        std::atomic_int produced{0};ControlledCoordinator co(&l);co.produced=&produced;
        QSignalSpy done(&co,&ImportCoordinator::artworkCompleted),ready(&co,&ImportCoordinator::shutdownReady);QVERIFY(co.repairLegacyArtwork());
        // Deliberately do not pump owner events until the result is queued.
        for(int i=0;i<100&&produced.load()==0;++i)QTest::qSleep(20);QCOMPARE(produced.load(),1);QTest::qSleep(200);QCOMPARE(produced.load(),1);QVERIFY(co.isArtworkBusy());QVERIFY(!co.isBusy());
        co.shutdown();QTRY_COMPARE(ready.size(),1);QCOMPARE(done.size(),1);QVERIFY(summary(done).cancelled);QCOMPARE(summary(done).recovered,0);QCOMPARE(cacheFiles(l),files);QCOMPARE(l.track(t.id).cover,t.cover);QVERIFY(!co.repairLegacyArtwork());
    }
    void destroyingCoordinatorCancelsWithoutLateCommit(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"destroy",Qt::red));QVERIFY(t.id);const auto files=cacheFiles(l);
        QSemaphore reached;auto state=std::make_shared<std::atomic_bool>(false);auto co=std::make_unique<ControlledCoordinator>(&l);co->libraryRoot=l.root();
        co->script=[&reached,state](ScriptedRepair &w){const auto r=Importer::repairArtwork(w.root,w.tracks[0]);reached.release();while(!w.token->load())QThread::msleep(5);w.repaired(r);w.finished();state->store(true);};
        QVERIFY(co->repairLegacyArtwork());QVERIFY(reached.tryAcquire(1,5000));co.reset();QTRY_VERIFY(state->load());QTest::qWait(50);QCOMPARE(cacheFiles(l),files);QCOMPARE(l.track(t.id).cover,t.cover);
    }
    void oldSchemaRecoveryPersistsAndProtectsUnrelatedData(){
        QTemporaryDir d;const auto root=d.filePath("data");qint64 id=0,list=0;QString audioPath;QByteArray audio,lyrics;
        {Library l(root);QVERIFY(l.open());auto t=legacy(l,embedded(d.path(),"old-schema",Qt::red));QVERIFY(t.id);id=t.id;audioPath=t.file;audio=bytes(l.absolute(t.file));
         lyrics="[00:01.00]Keep this lyric\n";QVERIFY(l.saveLyricsPreservingText(id,lyrics));QVERIFY(l.favorite(id,true));list=l.createPlaylist("old list");QVERIFY(l.addToPlaylist(list,{id}));auto session=l.startSession(id,1000);QVERIFY(l.saveSession(session,800,80));
         QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec("ALTER TABLE tracks DROP COLUMN cover_origin"));}
        QString recovered;QByteArray cache;
        {Library l(root);QVERIFY(l.open());QVERIFY(l.track(id).coverOrigin.isEmpty());const auto schema=import_fixture::schema(l);ImportCoordinator co(&l);QSignalSpy done(&co,&ImportCoordinator::artworkCompleted);QVERIFY(co.repairLegacyArtwork());QTRY_COMPARE(done.size(),1);QCOMPARE(summary(done).recovered,1);
         const auto t=l.track(id);recovered=t.cover;cache=bytes(l.absolute(t.cover));QCOMPARE(t.file,audioPath);QCOMPARE(bytes(l.absolute(t.file)),audio);QCOMPARE(bytes(l.absolute(t.lyrics)),lyrics);QVERIFY(t.favorite);QCOMPARE(t.plays,qint64(1));QCOMPARE(t.heard,qint64(800));QCOMPARE(l.tracks(list).size(),1);QCOMPARE(import_fixture::schema(l),schema);}
        {Library l(root);QVERIFY(l.open());QCOMPARE(l.track(id).cover,recovered);QCOMPARE(bytes(l.absolute(recovered)),cache);ImportCoordinator co(&l);QVERIFY(!co.repairLegacyArtwork());}
    }
    void cachedImageRequestsObserveChangedIdentity(){
        QTemporaryDir d;const auto file=d.filePath("cache.png");QVERIFY(write(file,png(Qt::red)));
        UiImages provider;const auto id="artwork?file="+QString::fromUtf8(QUrl::toPercentEncoding(QUrl::fromLocalFile(file).toString()))+"&seed=same";
        const auto first=provider.requestImage(id,nullptr,QSize(96,96));QVERIFY(closeColor(first,Qt::red));QVERIFY(closeColor(coverPixmap(file,"same",96).toImage(),Qt::red));
        QTest::qWait(3);QVERIFY(write(file,png(Qt::green)));QVERIFY(closeColor(provider.requestImage(id,nullptr,QSize(96,96)),Qt::green));QVERIFY(closeColor(coverPixmap(file,"same",96).toImage(),Qt::green));
        QFileInfo before(file);const auto time=before.lastModified();QVERIFY(!provider.requestImage(id,nullptr,QSize(96,96)).isNull());QVERIFY(QFileInfo(file).lastModified()==time);
    }
    void qmlAutomaticRecoveryRefreshesWithoutRestartAndSwitches(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=legacy(l,embedded(d.path(),"qml-a",Qt::red));auto b=legacy(l,embedded(d.path(),"qml-b",Qt::green,660));QVERIFY(a.id&&b.id);
        QVERIFY(l.setSetting("reduceMotion",true));QmlShell shell(&l,nullptr,true);QSignalSpy done(shell.importCoordinator(),&ImportCoordinator::artworkCompleted);QVERIFY(shell.load());
        shell.facade()->play({a.id,b.id},a.id);QSignalSpy art(shell.playbackBridge(),&PlaybackBridge::artworkChanged);
        shell.feedbackBridge()->post("Playback","Error","保留正在显示的错误","raw diagnostic");const auto visibleEvent=shell.feedbackBridge()->current()["eventId"].toString();
        QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);QCOMPARE(summary(done).recovered,2);QVERIFY(!art.isEmpty());
        const auto path=l.absolute(l.track(a.id).cover);QTRY_COMPARE(shell.playbackBridge()->coverUrl().toLocalFile(),path);QTRY_COMPARE(shell.lyricsModel()->cover().toLocalFile(),path);
        auto *small=c4c5_fixture::visual(shell.window()->contentItem(),"bottomCover"),*large=c4c5_fixture::visual(shell.window()->contentItem(),"currentArtwork");QVERIFY(small&&large);QTRY_COMPARE(small->property("file").toUrl().toLocalFile(),path);QTRY_COMPARE(large->property("file").toUrl().toLocalFile(),path);
        bool found=false;for(int i=0;i<shell.libraryModel()->rowCount();++i){const auto ix=shell.libraryModel()->index(i,0);if(shell.libraryModel()->data(ix,TrackListModel::TrackKey).toString()==QString::number(a.id)){found=true;QCOMPARE(shell.libraryModel()->data(ix,TrackListModel::Cover).toUrl().toLocalFile(),path);}}QVERIFY(found);
        QVERIFY(closeColor(qmlImage(path),Qt::red));QVERIFY(closeColor(coverPixmap(path,a.title,96).toImage(),Qt::red));
        QCOMPARE(shell.feedbackBridge()->current()["eventId"].toString(),visibleEvent);QCOMPARE(shell.feedbackBridge()->history().size(),2);QVERIFY(shell.feedbackBridge()->history()[0].toMap()["silent"].toBool());
        shell.facade()->play({a.id,b.id},b.id);const auto bpath=l.absolute(l.track(b.id).cover);QTRY_COMPARE(shell.playbackBridge()->coverUrl().toLocalFile(),bpath);QVERIFY(closeColor(qmlImage(bpath),Qt::green));
        QVERIFY(shell.warnings().isEmpty());QVERIFY(shell.shutdown());
    }
    void qmlCloseWaitsForArtworkOnlyAndStopsProcess(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto wav=d.filePath("close.wav");QVERIFY(import_fixture::wave(wav));auto t=legacy(l,wav);QVERIFY(t.id);QVERIFY(l.setSetting("closeToTray",false));
        const auto mock=d.filePath("mock");QVERIFY(QDir().mkpath(mock));const auto fixture=QCoreApplication::applicationDirPath()+"/relaxer_import_process_fixture.exe";QVERIFY(QFile::copy(fixture,mock+"/ffprobe.exe"));
        const auto prior=qgetenv("RELAXER_FFMPEG_DIR"),marker=qgetenv("RELAXER_IMPORT_PROCESS_MARKER");const auto reset=qScopeGuard([&]{if(prior.isNull())qunsetenv("RELAXER_FFMPEG_DIR");else qputenv("RELAXER_FFMPEG_DIR",prior);if(marker.isNull())qunsetenv("RELAXER_IMPORT_PROCESS_MARKER");else qputenv("RELAXER_IMPORT_PROCESS_MARKER",marker);});
        qputenv("RELAXER_FFMPEG_DIR",mock.toUtf8());qputenv("RELAXER_IMPORT_PROCESS_MARKER",d.filePath("started.txt").toUtf8());
        QmlShell shell(&l,nullptr,true);QVERIFY(shell.load());QSignalSpy ready(shell.importBridge(),&ImportBridge::readyToClose);QTRY_VERIFY(QFile::exists(d.filePath("started.txt")));QVERIFY(shell.importBridge()->artworkBusy());QVERIFY(!shell.importBridge()->busy());QVERIFY(shell.importBridge()->unfinished());
        shell.appBridge()->navigate(1);shell.appBridge()->navigate(0); // Page lifetime is independent of recovery.
        QVERIFY(!shell.window()->close());QVERIFY(shell.window()->property("awaitingImportClose").toBool());QTRY_COMPARE_WITH_TIMEOUT(ready.size(),1,8000);QTRY_VERIFY(!shell.window()->isVisible());QVERIFY(shell.importCoordinator()->isIdle());QCOMPARE(l.track(t.id).cover,t.cover);QVERIFY(shell.warnings().isEmpty());
    }
};
QTEST_MAIN(ArtworkRecoveryTests)
#include "artwork-recovery-tests.moc"
