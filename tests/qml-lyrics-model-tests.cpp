#include "ui-bridge/lyrics-model.h"
#include <QAbstractItemModelTester>
#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
namespace {
qint64 fixture(Library &lib,const QString &hash,const QByteArray &lyrics={}) {
    QFile audio(lib.absolute("Music/"+hash+".wav"));if(!audio.open(QIODevice::WriteOnly))return 0;
    QDataStream out(&audio);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(320036);out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(320000);
    QByteArray pcm(320000,'\0');out.writeRawData(pcm.data(),pcm.size());audio.close();
    Track t;t.hash=hash;t.file="Music/"+hash+".wav";t.title=hash;t.duration=20000;const auto id=lib.insert(t);
    if(!lyrics.isEmpty()&&!lib.saveLyrics(id,lyrics))return 0;return id;
}
}
class QmlLyricsModelTests:public QObject {
    Q_OBJECT
private slots:
    void emptyPlainAndDuplicateTimestamps() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto id=fixture(lib,"first");QVERIFY(id);
        PlaybackFacade player(&lib,nullptr,true);LyricsModel model(&lib,&player);
        QAbstractItemModelTester tester(&model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        player.play({id},id);QTRY_COMPARE(model.trackKey(),QString::number(id));QCOMPARE(model.count(),0);QVERIFY(!model.timed());
        QVERIFY(lib.saveLyrics(id,QString("中文\n日本語 テスト\nEnglish\n").toUtf8()));QTRY_COMPARE(model.count(),3);QVERIFY(!model.timed());QCOMPARE(model.currentIndex(),-1);
        QVERIFY(lib.saveLyrics(id,QString("[00:01.00]Hello\n[00:01.00]你好\n[00:02.00]World\n[00:03.00]End\n").toUtf8()));
        QTRY_VERIFY(model.timed());QCOMPARE(model.count(),3);
        QCOMPARE(model.data(model.index(0),LyricsModel::Original).toString(),QString("Hello"));QCOMPARE(model.data(model.index(0),LyricsModel::Translation).toString(),QString("你好"));
        player.toggle();
        emit player.positionChanged(999,20000);QCOMPARE(model.currentIndex(),-1);
        emit player.positionChanged(1000,20000);QCOMPARE(model.currentIndex(),0);
        emit player.positionChanged(2000,20000);QCOMPARE(model.currentIndex(),1);
        QVERIFY(model.data(model.index(0),LyricsModel::Played).toBool());QVERIFY(model.data(model.index(1),LyricsModel::Current).toBool());
        emit player.positionChanged(0,20000);QCOMPARE(model.currentIndex(),-1);
        QVERIFY(lib.saveLyrics(id,"[00:03.00]Late\n[00:01.00]Early\n[00:02.00]Middle\n"));
        QTRY_COMPARE(model.data(model.index(0),LyricsModel::Original).toString(),QString("Early"));
        emit player.positionChanged(2999,20000);QCOMPARE(model.currentIndex(),1);
        emit player.positionChanged(3000,20000);QCOMPARE(model.currentIndex(),2);
        QVERIFY(player.shutdown());
    }
    void offsetSeekAndPerTrackOwnership() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());
        const auto a=fixture(lib,"a","[00:01.00]One\n[00:04.00]Two\n");const auto b=fixture(lib,"b","[00:02.00]Other\n");QVERIFY(a&&b);
        PlaybackFacade player(&lib,nullptr,true);LyricsModel model(&lib,&player);player.play({a,b},a);
        QTRY_COMPARE(model.count(),2);QTRY_VERIFY(player.seekable());player.toggle();
        QVERIFY(model.setOffsetMs(5000));QCOMPARE(lib.lyricOffset(a),5000);emit player.positionChanged(5999,20000);QCOMPARE(model.currentIndex(),-1);
        emit player.positionChanged(6000,20000);QCOMPARE(model.currentIndex(),0);
        QVERIFY(!model.setOffsetMs(5001));QCOMPARE(lib.lyricOffset(a),5000);
        QVERIFY(model.setOffsetMs(-5000));emit player.positionChanged(0,20000);QCOMPARE(model.currentIndex(),1);
        QVERIFY(model.setOffsetMs(10));QVERIFY(model.seekRow(1));QTRY_VERIFY(qAbs(player.position()-4010)<300);
        player.play({a,b},b);QTRY_COMPARE(model.trackKey(),QString::number(b));QCOMPARE(model.offsetMs(),0);QCOMPARE(model.count(),1);
        QVERIFY(model.setOffsetMs(-10));QCOMPARE(lib.lyricOffset(a),10);QCOMPARE(lib.lyricOffset(b),-10);
        QVERIFY(!model.seekRow(999));QVERIFY(player.shutdown());
    }
    void previewIsReadOnlyAndTrackBound() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const QByteArray source="[00:01.00]Hello moon\n[00:02.00]Hello sun\n";
        const auto a=fixture(lib,"a",source),b=fixture(lib,"b","[00:01.00]Different\n");QVERIFY(a&&b);
        PlaybackFacade player(&lib,nullptr,true);LyricsModel model(&lib,&player);player.play({a,b},a);QTRY_COMPARE(model.count(),2);
        TranslationSummary summary;summary.trackId=a;summary.trackHash=lib.track(a).hash;summary.prepared=ai::lyrics::prepare(source,"a",{});
        QVERIFY(summary.prepared.ok());summary.sourceDigest=summary.prepared.sourceDigest;summary.translations={"月亮你好","太阳你好"};summary.state=TranslationState::CompletedPendingSave;
        const auto before=LyricsDocumentReader::readFile(lib.absolute(lib.track(a).lyrics)).bytes;
        model.presentTranslation(summary);QVERIFY(model.preview());QCOMPARE(model.data(model.index(0),LyricsModel::Translation).toString(),QString("月亮你好"));
        QCOMPARE(LyricsDocumentReader::readFile(lib.absolute(lib.track(a).lyrics)).bytes,before);
        player.play({a,b},b);QTRY_COMPARE(model.trackKey(),QString::number(b));model.presentTranslation(summary);QVERIFY(!model.preview());QCOMPARE(model.count(),1);
        player.play({a,b},a);QTRY_COMPARE(model.trackKey(),QString::number(a));model.presentTranslation(summary);QVERIFY(model.preview());
        summary.state=TranslationState::Cancelled;model.presentTranslation(summary);QVERIFY(!model.preview());QCOMPARE(model.data(model.index(0),LyricsModel::Translation).toString(),QString());
        QVERIFY(player.shutdown());
    }
    void noResetOnProgressAndSafeDestruction() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());QByteArray source;
        for(int i=0;i<5000;++i)source+=QString("[%1:%2.00]Long lyric %3 中文 日本語\n").arg(i/60,2,10,QChar('0')).arg(i%60,2,10,QChar('0')).arg(i).toUtf8();
        const auto id=fixture(lib,"long",source);QVERIFY(id);
        auto player=std::make_unique<PlaybackFacade>(&lib,nullptr,true);auto model=std::make_unique<LyricsModel>(&lib,player.get());player->play({id},id);
        QTRY_COMPARE(model->count(),5000);QSignalSpy resets(model.get(),&QAbstractItemModel::modelReset);
        QTRY_VERIFY(player->position()>500);QCOMPARE(resets.size(),0);
        model->reload();model.reset();QCoreApplication::processEvents();QVERIFY(player->shutdown());player.reset();
        QCOMPARE(QSqlDatabase::connectionNames().size(),1);
    }
    void failedFileAndReadOnlyApi() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto id=fixture(lib,"target");QVERIFY(id);
        PlaybackFacade player(&lib,nullptr,true);LyricsModel model(&lib,&player);player.play({id},id);QTRY_COMPARE(model.trackKey(),QString::number(id));
        const auto path=dir.filePath("歌词 with spaces.lrc");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("[00:01.00]Imported\n");file.close();
        QCOMPARE(model.metaObject()->indexOfMethod("importLocalLyrics(QUrl)"),-1);
        QVERIFY(lib.saveLyricsPreservingText(id,"[00:01.00]Imported\n"));model.reload();QTRY_COMPARE(model.count(),1);
        QVERIFY(QFile::remove(lib.absolute(lib.track(id).lyrics)));model.reload();QTRY_VERIFY(!model.error().isEmpty());QCOMPARE(model.count(),0);QVERIFY(player.shutdown());
    }
};
QTEST_GUILESS_MAIN(QmlLyricsModelTests)
#include "qml-lyrics-model-tests.moc"
