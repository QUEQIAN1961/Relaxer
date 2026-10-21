#include "c4c5-ui-fixture.h"
#include <QAbstractItemModelTester>
using namespace c4c5_fixture;
namespace {
class EmptyReply final:public QNetworkReply {
public:
    EmptyReply(const QNetworkRequest &request,QObject *parent,QByteArray body="[]"):QNetworkReply(parent),m_body(std::move(body)){setRequest(request);setUrl(request.url());open(QIODevice::ReadOnly);QTimer::singleShot(0,this,[this]{if(!isFinished()){setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);setFinished(true);emit readyRead();emit finished();}});}
    void abort()override{if(isFinished())return;setError(OperationCanceledError,"cancelled");setFinished(true);emit finished();}
    qint64 bytesAvailable()const override{return m_body.size()-offset+QNetworkReply::bytesAvailable();}
protected:qint64 readData(char *data,qint64 max)override{const auto count=qMin(max,m_body.size()-offset);if(count<=0)return -1;memcpy(data,m_body.constData()+offset,size_t(count));offset+=count;return count;}
private:qint64 offset=0;QByteArray m_body;
};
class FakeNetwork final:public QNetworkAccessManager {
public:int calls=0;QByteArray body="[]";
protected:QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *)override{++calls;return new EmptyReply(request,this,body);}
};
QJsonObject record(const Track &t){return {{"id",1234},{"trackName",t.title},{"artistName",t.artist},{"albumName",t.album},{"duration",60},
    {"syncedLyrics","[00:01.00]Hello world\n[00:04.00]Walk home\n[00:50.00]Goodnight moon\n"},{"source","Controlled cache"}};}
}
class QmlLyricsSearchBridgeTests:public QObject {
    Q_OBJECT
private slots:
    void selectedBatchKeepsFullReviewListAndOnlySearchesChosenTracks(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());
        const auto a=seed(lib,"A","selected-a"),b=seed(lib,"B","selected-b"),c=seed(lib,"C","selected-c");
        FakeNetwork network;PlaybackFacade player(&lib,nullptr,true);
        LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);LyricsSearchBridge model(&lib,&player,&service);
        const QStringList keys{QString::number(a.id),QString::number(b.id)};QVERIFY(model.setTargetTracks(keys,false,true));
        QVERIFY(!model.searchTracks({},true));QVERIFY(!model.searchTracks({QString::number(c.id)},true));
        QVERIFY(!model.searchTracks({QString::number(b.id)},false));QCOMPARE(network.calls,0);
        QList<qint64> events;connect(&service,&LyricsSearchCoordinator::eventOccurred,this,[&](const auto &event){events.append(event.trackId);});
        QVERIFY(model.searchTracks({QString::number(b.id),QString::number(b.id)},true));
        QCOMPARE(service.summary().requested,1);QCOMPARE(model.targetTracks().size(),2);
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),5000);QVERIFY(!events.isEmpty());
        for(auto id:events)QCOMPARE(id,b.id);
        QCOMPARE(model.targetTracks()[0].toMap()["trackKey"].toString(),keys[0]);
        QCOMPARE(model.targetTracks()[1].toMap()["trackKey"].toString(),keys[1]);service.shutdown();QVERIFY(player.shutdown());
    }
    void missingTranslationUsesLocalAssessmentAndHumanJudgment(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());
        const auto a=seed(lib,"Foreign","foreign"),b=seed(lib,"Bilingual","bilingual"),c=seed(lib,"Missing","missing"),d=seed(lib,"中文","chinese");
        QVERIFY(lib.saveLyrics(a.id,"[00:01.00]Hello my friend\n[00:03.00]We walk together\n"));
        QVERIFY(lib.saveLyrics(b.id,QString("[00:01.00]Hello friend\n[00:01.00]朋友你好\n[00:02.00]Goodbye friend\n[00:02.00]朋友再见\n").toUtf8()));
        QVERIFY(lib.saveLyrics(d.id,QString("[00:01.00]今天的风很温柔\n[00:03.00]我们一起走回家\n").toUtf8()));
        FakeNetwork network;PlaybackFacade player(&lib,nullptr,true);LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);
        LyricsSearchBridge model(&lib,&player,&service);
        QVERIFY(model.setTargetTracks({QString::number(a.id),QString::number(b.id),QString::number(c.id),QString::number(d.id)},false,true));
        auto rows=model.targetTracks();QVERIFY(rows[0].toMap()["translationMissing"].toBool());
        for(int i=1;i<4;++i)QVERIFY(!rows[i].toMap()["translationMissing"].toBool());
        QVERIFY(lib.setTranslationJudgment(a.id,TranslationJudgment::NoTranslationNeeded));QVERIFY(!model.targetTracks()[0].toMap()["translationMissing"].toBool());
        QVERIFY(lib.setTranslationJudgment(c.id,TranslationJudgment::NeedsTranslation));QVERIFY(model.targetTracks()[2].toMap()["translationMissing"].toBool());
        QVERIFY(lib.setTranslationJudgment(a.id,TranslationJudgment::Unknown));
        QVERIFY(lib.saveLyrics(a.id,QString("[00:01.00]Hello friend\n[00:01.00]朋友你好\n[00:02.00]Goodbye friend\n[00:02.00]朋友再见\n").toUtf8()));
        QVERIFY(!model.targetTracks()[0].toMap()["translationMissing"].toBool());QCOMPARE(network.calls,0);service.shutdown();QVERIFY(player.shutdown());
    }
    void reviewUnsavedReplacementStaysOrangeAfterRestart(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto t=seed(lib,"Alpha","review-existing");QVERIFY(wave(lib.absolute(t.file)));
        QVERIFY(lib.saveLyrics(t.id,"[00:01.00]Existing local lyrics\n"));FakeNetwork network;network.body=QJsonDocument(QJsonArray{record(t)}).toJson(QJsonDocument::Compact);PlaybackFacade player(&lib,nullptr,true);
        {
            LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);LyricsSearchBridge model(&lib,&player,&service);
            QVERIFY(model.setTargetTracks({QString::number(t.id)},false,true));QVERIFY(model.search(true));
            QTRY_VERIFY_WITH_TIMEOUT(!model.busy(),5000);QCOMPARE(model.targetTracks()[0].toMap()["searchState"].toString(),QString("choice"));service.shutdown();
        }
        const int calls=network.calls;
        {
            LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);LyricsSearchBridge model(&lib,&player,&service);
            QVERIFY(model.setTargetTracks({QString::number(t.id)},true,true));QCOMPARE(network.calls,calls);
            QCOMPARE(model.targetTracks()[0].toMap()["searchState"].toString(),QString("choice"));QVERIFY(model.select(0));QVERIFY(model.saveSelected(model.selectedKey(),true));
            QCOMPARE(model.targetTracks()[0].toMap()["searchState"].toString(),QString("complete"));service.shutdown();
        }
        QVERIFY(player.shutdown());
    }
    void reviewCompletedAndChoiceStatesSurviveBridgeRecreation(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());
        const auto a=seed(lib,"Saved","saved"),b=seed(lib,"Choice","choice"),c=seed(lib,"Unfinished","unfinished");
        QVERIFY(lib.saveLyrics(a.id,"[00:01.00]Saved lyrics\n"));QVERIFY(lib.saveLookupCache(b.id,QJsonArray{record(b)},false));
        const QStringList keys{QString::number(a.id),QString::number(b.id),QString::number(c.id)};
        FakeNetwork network;PlaybackFacade player(&lib,nullptr,true);
        {
            LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);LyricsSearchBridge model(&lib,&player,&service);
            QVERIFY(model.setTargetTracks(keys,true,true));auto rows=model.targetTracks();QCOMPARE(rows.size(),3);
            QCOMPARE(rows[0].toMap()["searchState"].toString(),QString("complete"));QCOMPARE(rows[1].toMap()["searchState"].toString(),QString("choice"));QCOMPARE(rows[2].toMap()["searchState"].toString(),QString("incomplete"));QCOMPARE(network.calls,0);
            QVERIFY(model.setTargetTrack(QString::number(b.id)));QVERIFY(model.select(0));QVERIFY(model.saveSelected(model.selectedKey(),true));
            QCOMPARE(model.targetTracks()[1].toMap()["searchState"].toString(),QString("complete"));service.shutdown();
        }
        {
            LyricsSearchCoordinator service(&lib,nullptr,QUrl("https://mock.invalid/api"),{},&network);LyricsSearchBridge model(&lib,&player,&service);
            QVERIFY(model.setTargetTracks(keys,true,true));QCOMPARE(model.targetTracks()[1].toMap()["searchState"].toString(),QString("complete"));QCOMPARE(network.calls,0);
            bool searching=false;connect(&model,&LyricsSearchBridge::changed,this,[&]{for(const auto &row:model.targetTracks())searching|=row.toMap()["searchState"].toString()=="searching";});
            QVERIFY(model.searchTargets(true));QTRY_VERIFY_WITH_TIMEOUT(searching,3000);model.stop();QVERIFY(!model.busy());service.shutdown();
        }
        QVERIFY(player.shutdown());QCOMPARE(QSqlDatabase::connectionNames().size(),1);
    }
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
