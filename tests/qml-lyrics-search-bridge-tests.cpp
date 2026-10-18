#include "c4c5-ui-fixture.h"
#include <QAbstractItemModelTester>
using namespace c4c5_fixture;
namespace {
class EmptyReply final:public QNetworkReply {
public:
    EmptyReply(const QNetworkRequest &request,QObject *parent):QNetworkReply(parent){setRequest(request);setUrl(request.url());open(QIODevice::ReadOnly);QTimer::singleShot(0,this,[this]{if(!isFinished()){setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);setFinished(true);emit readyRead();emit finished();}});}
    void abort()override{if(isFinished())return;setError(OperationCanceledError,"cancelled");setFinished(true);emit finished();}
    qint64 bytesAvailable()const override{return 2-offset+QNetworkReply::bytesAvailable();}
protected:qint64 readData(char *data,qint64 max)override{const auto count=qMin(max,2-offset);if(count<=0)return -1;memcpy(data,"[]"+offset,size_t(count));offset+=count;return count;}
private:qint64 offset=0;
};
class FakeNetwork final:public QNetworkAccessManager {
public:int calls=0;
protected:QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *)override{++calls;return new EmptyReply(request,this);}
};
QJsonObject record(const Track &t){return {{"id",1234},{"trackName",t.title},{"artistName",t.artist},{"albumName",t.album},{"duration",60},
    {"syncedLyrics","[00:01.00]Hello world\n[00:04.00]Walk home\n[00:50.00]Goodnight moon\n"},{"source","Controlled cache"}};}
}
class QmlLyricsSearchBridgeTests:public QObject {
    Q_OBJECT
private slots:
    void cachedCandidatesStayLocalAndRequireExactSaveConsent(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto track=seed(lib,"Alpha","alpha");QVERIFY(wave(lib.absolute(track.file)));
        const QByteArray original="[00:01.00]Existing lyric\n";QVERIFY(lib.saveLyrics(track.id,original));QVERIFY(lib.saveLookupCache(track.id,QJsonArray{record(track)},false));
        FakeNetwork network;PlaybackFacade player(&lib,nullptr,true);LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);
        LyricsSearchBridge model(&lib,&player,&service);QAbstractItemModelTester tester(&model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        player.play({track.id},track.id);QTRY_COMPARE(model.rowCount(),1);QCOMPARE(network.calls,0);QVERIFY(model.select(0));const auto key=model.selectedKey();QVERIFY(!key.isEmpty());
        QVERIFY(!model.saveSelected(key,false));QCOMPARE(LyricsDocumentReader::readFile(lib.absolute(lib.track(track.id).lyrics)).bytes,original);
        QVERIFY(!model.saveSelected("wrong",true));QVERIFY(lib.saveLyrics(track.id,"[00:01.00]Externally changed\n"));
        QVERIFY(!model.saveSelected(key,true));QVERIFY(LyricsDocumentReader::readFile(lib.absolute(lib.track(track.id).lyrics)).bytes.contains("Externally"));
        QVERIFY(model.select(0));QVERIFY(model.saveSelected(model.selectedKey(),true));QVERIFY(LyricsDocumentReader::readFile(lib.absolute(lib.track(track.id).lyrics)).bytes.contains("Walk home"));
        QCOMPARE(network.calls,0);service.shutdown();QVERIFY(player.shutdown());
    }
    void explicitSearchUsesFakeTransportAndSwitchInvalidatesSelection(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto a=seed(lib,"Alpha","alpha"),b=seed(lib,"Beta","beta");
        QVERIFY(wave(lib.absolute(a.file)));QVERIFY(wave(lib.absolute(b.file)));QVERIFY(lib.saveLookupCache(a.id,QJsonArray{record(a)},false));
        FakeNetwork network;PlaybackFacade player(&lib,nullptr,true);LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);
        auto model=std::make_unique<LyricsSearchBridge>(&lib,&player,&service);player.play({a.id,b.id},a.id);QTRY_COMPARE(model->rowCount(),1);QVERIFY(model->select(0));const auto old=model->selectedKey();
        player.play({a.id,b.id},b.id);QTRY_COMPARE(model->rowCount(),0);QVERIFY(!model->saveSelected(old,true));QVERIFY(!model->search(false));QCOMPARE(network.calls,0);
        QVERIFY(model->search(true));QTRY_VERIFY(network.calls>0);model->stop();QVERIFY(!model->busy());model.reset();QCoreApplication::processEvents();service.shutdown();QVERIFY(player.shutdown());
        QVERIFY(lib.track(b.id).lyrics.isEmpty());QCOMPARE(QSqlDatabase::connectionNames().size(),1);
    }
};
QTEST_MAIN(QmlLyricsSearchBridgeTests)
#include "qml-lyrics-search-bridge-tests.moc"
