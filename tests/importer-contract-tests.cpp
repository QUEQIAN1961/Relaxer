#include "import-fixture.h"
#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace relaxer;

class ImporterContractTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qRegisterMetaType<ImportResult>();
        QVERIFY(!toolPath("ffmpeg").isEmpty());QVERIFY(!toolPath("ffprobe").isEmpty());
    }
    void scanCanonicalPathsAndIgnoreUnsupportedInputs() {
        QTemporaryDir dir;const auto root=dir.filePath("data"),inputs=dir.filePath("inputs");QVERIFY(QDir().mkpath(inputs));
        Library library(root);QVERIFY(library.open());const auto a=inputs+"/one.mid",b=inputs+"/two.mid";
        QVERIFY(import_fixture::midi(a,1));QVERIFY(import_fixture::midi(b,2));
        QFile unsupported(inputs+"/ignored.txt");QVERIFY(unsupported.open(QIODevice::WriteOnly));unsupported.write("fixture");unsupported.close();
        Importer worker(root,{a,a,inputs,inputs+"/missing.wav",unsupported.fileName()});
        QVector<ImportResult> received;
        connect(&worker,&Importer::imported,this,[&](const ImportResult &result){received << result;},Qt::QueuedConnection);
        QSignalSpy finished(&worker,&Importer::finished);worker.run();QCoreApplication::sendPostedEvents();
        QCOMPARE(finished.size(),1);QCOMPARE(finished.first().first().toBool(),false);QCOMPARE(received.size(),2);
        QSet<QString> sources;
        for(const auto &result:received){QVERIFY2(result.error.isEmpty(),qPrintable(result.error));sources.insert(result.source);QVERIFY(QFileInfo::exists(root+"/"+result.track.file));}
        QCOMPARE(sources,QSet<QString>({QFileInfo(a).canonicalFilePath(),QFileInfo(b).canonicalFilePath()}));
        QVERIFY(QFileInfo::exists(a));QVERIFY(QFileInfo::exists(b));
    }
    void contentDuplicateKeepsIdStatisticsAndPlaylistMembership() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto before=import_fixture::schema(library);const auto a=dir.filePath("one.mid"),b=dir.filePath("copy.mid");
        QVERIFY(import_fixture::midi(a));QVERIFY(QFile::copy(a,b));
        auto first=Importer::importOne(library.root(),a),copy=Importer::importOne(library.root(),b);QVERIFY(first.error.isEmpty());QVERIFY(copy.error.isEmpty());
        QCOMPARE(first.track.hash,copy.track.hash);QCOMPARE(first.track.file,copy.track.file);
        first.track.title="Preserved title";const auto id=library.insert(first.track);QVERIFY(id>0);
        auto session=library.startSession(id,1000);QVERIFY(library.saveSession(session,900,80));QVERIFY(library.favorite(id,true));
        const auto p=library.createPlaylist("first"),q=library.createPlaylist("second");QVERIFY(library.addToPlaylist(p,{id}));
        const auto duplicate=library.insert(copy.track);QCOMPARE(duplicate,id);QVERIFY(library.addToPlaylist(q,{duplicate}));QVERIFY(library.addToPlaylist(q,{duplicate}));
        QCOMPARE(library.tracks().size(),1);QCOMPARE(library.tracks(q).size(),1);QCOMPARE(library.track(id).title,QString("Preserved title"));
        QCOMPARE(library.track(id).heard,qint64(900));QCOMPARE(library.track(id).plays,qint64(1));QVERIFY(library.track(id).favorite);
        QCOMPARE(import_fixture::schema(library),before);
    }
    void invalidMediaIsLocalFailureAndCoverFailureIsOptional() {
        QTemporaryDir dir;const auto root=dir.filePath("data"),good=dir.filePath("song.wav"),bad=dir.filePath("bad.mp3");
        QVERIFY(import_fixture::wave(good));QFile file(bad);QVERIFY(file.open(QIODevice::WriteOnly));file.write("not media");file.close();
        QFile cover(dir.filePath("song.png"));QVERIFY(cover.open(QIODevice::WriteOnly));cover.write("invalid cover");cover.close();
        const auto ok=Importer::importOne(root,good);QVERIFY2(ok.error.isEmpty(),qPrintable(ok.error));QVERIFY(ok.track.cover.isEmpty());
        QCOMPARE(ok.track.coverOrigin,QString("none"));QCOMPARE(ok.track.title,QString("Fixture Title"));
        QVERIFY(!Importer::importOne(root,bad).error.isEmpty());QVERIFY(QFileInfo::exists(good));QVERIFY(QFileInfo::exists(bad));
    }
    void preCancelledBatchAndInFlightCancellationKeepExistingSemantics() {
        QTemporaryDir dir;QStringList sources;
        for(int i=0;i<32;++i){const auto path=dir.filePath(QString::number(i)+".mid");QVERIFY(import_fixture::midi(path,i));sources<<path;}
        Importer before(dir.filePath("before"),sources);QSignalSpy none(&before,&Importer::imported),end(&before,&Importer::finished);
        before.cancel();before.run();QCOMPARE(none.size(),0);QCOMPARE(end.size(),1);QCOMPARE(end.first().first().toBool(),true);
        Importer running(dir.filePath("after"),sources);std::atomic_int produced{0};
        connect(&running,&Importer::imported,&running,[&](const ImportResult &){if(produced.fetch_add(1)==0)running.cancel();},Qt::DirectConnection);
        QSignalSpy done(&running,&Importer::finished);running.run();QVERIFY(produced.load()>0);QVERIFY(produced.load()<sources.size());
        QCOMPARE(done.size(),1);QCOMPARE(done.first().first().toBool(),true);
    }
    void databaseFailureDoesNotRemoveProcessedOrOriginalFiles() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto source=dir.filePath("valid.mid");QVERIFY(import_fixture::midi(source));const auto result=Importer::importOne(library.root(),source);QVERIFY(result.error.isEmpty());
        QSqlQuery query(import_fixture::database(library));QVERIFY(query.exec("CREATE TRIGGER reject_import BEFORE INSERT ON tracks BEGIN SELECT RAISE(ABORT,'fixture rejection'); END"));
        QCOMPARE(library.insert(result.track),qint64(0));QVERIFY(library.lastError().contains("fixture rejection"));
        QVERIFY(library.tracks().isEmpty());QVERIFY(QFileInfo::exists(source));QVERIFY(QFileInfo::exists(library.absolute(result.track.file)));
        QVERIFY(query.exec("DROP TRIGGER reject_import"));QVERIFY(library.insert(result.track)>0);
    }
    void artworkLyricsAndDatabasePersistAfterReopen() {
        QTemporaryDir dir;const auto source=dir.filePath("cover.wav"),root=dir.filePath("data");QVERIFY(import_fixture::wave(source));
        QImage image(24,24,QImage::Format_RGB32);image.fill(Qt::green);QVERIFY(image.save(dir.filePath("cover.png")));
        QFile lyric(dir.filePath("cover.lrc"));QVERIFY(lyric.open(QIODevice::WriteOnly));lyric.write(QStringLiteral("[00:00.00]她在遠方").toUtf8());lyric.close();
        qint64 id=0,p=0;QStringList before;
        { Library library(root);QVERIFY(library.open());before=import_fixture::schema(library);
          const auto result=Importer::importOne(root,source);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
          QCOMPARE(result.track.coverOrigin,QString("sidecar"));QCOMPARE(result.track.cover,"Artwork/"+result.track.hash+"-v2.png");
          QCOMPARE(result.track.album,QString("Fixture Album"));QVERIFY(result.track.duration>=190&&result.track.duration<=210);
          QVERIFY(!result.track.lyrics.isEmpty());id=library.insert(result.track);QVERIFY(id>0);p=library.createPlaylist("Persistent");QVERIFY(library.addToPlaylist(p,{id})); }
        { Library library(root);QVERIFY(library.open());QCOMPARE(library.tracks(p).size(),1);QCOMPARE(library.tracks(p).first().id,id);
          const auto track=library.track(id);QCOMPARE(track.title,QString("Fixture Title"));QVERIFY(QFileInfo::exists(library.absolute(track.cover)));
          QFile lyric(library.absolute(track.lyrics));QVERIFY(lyric.open(QIODevice::ReadOnly));QVERIFY(QString::fromUtf8(lyric.readAll()).contains(QStringLiteral("她在远方")));
          QCOMPARE(import_fixture::schema(library),before); }
    }
};
QTEST_GUILESS_MAIN(ImporterContractTests)
#include "importer-contract-tests.moc"
