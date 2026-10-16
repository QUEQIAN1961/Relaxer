#include "lyricslookup.h"
#include "domain/lyric-match.h"
#include "lyric-match-fixture.h"
#include "lyric-text.h"
#include "lookup-drive.h"
#include <QtTest>
#include <QDataStream>
#include <QElapsedTimer>
#include <QListWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <future>
#include <vector>
using namespace relaxer;
namespace f=lyric_match_fixture;
class RejectionServer:public QTcpServer {
  public:
    QJsonObject candidate;int requests=0;
    RejectionServer(){connect(this,&QTcpServer::newConnection,this,[this]{auto *s=nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,s,[this,s]{const auto bytes=s->property("buffer").toByteArray()+s->readAll();s->setProperty("buffer",bytes);if(!bytes.contains("\r\n\r\n")||s->property("answered").toBool())return;s->setProperty("answered",true);++requests;const bool get=bytes.split(' ').value(1).contains("/get");const auto body=get?QByteArray("{}"):QJsonDocument(QJsonArray{candidate}).toJson();s->write(QByteArray("HTTP/1.1 ")+(get?"404":"200")+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();});});}
    QUrl endpoint()const{return QUrl(QString("http://127.0.0.1:%1/api").arg(serverPort()));}
};
class LyricMatchCompatTests:public QObject {
    Q_OBJECT
  private slots:
    void initTestCase(){QCOMPARE(f::root()["cases"].toArray().size(),104);}
    void frozenAdapters_data(){QTest::addColumn<QJsonObject>("row");for(const auto &v:f::root()["cases"].toArray()){const auto row=v.toObject();QTest::newRow(qPrintable(row["id"].toString()))<<row;}}
    void frozenAdapters(){
        QFETCH(QJsonObject,row);const auto t=f::track(row["track"].toObject());const auto r=row["record"].toObject();const auto e=row["expected"].toObject();
        QCOMPARE(LyricsLookup::normalized(t.title),e["normalized"].toString());QCOMPARE(LyricsLookup::cleanTitle(t.title),e["cleanTitle"].toString());QCOMPARE(f::metadata(LyricsLookup::searchTrack(t)),e["searchTrack"].toObject());
        LyricsLookup::resetMatchScoreCount();QCOMPARE(LyricsLookup::matchScore(t,r),e["score"].toString().toDouble());QCOMPARE(LyricsLookup::matchScoreCount(),qint64(1));
        QCOMPARE(LyricsLookup::confident(t,r),e["confident"].toBool());QCOMPARE(LyricsLookup::matchReason(t,r),e["matchReason"].toString());
        QString reason="sentinel";QCOMPARE(LyricsLookup::credibleLyrics(f::lyrics(row),t,&reason),e["credible"].toBool());QCOMPARE(reason,e["rejection"].toString());
        const auto credit=originalCredit(t.title);QCOMPARE(QJsonObject({{"found",credit.found},{"title",credit.title},{"artist",credit.artist}}),e["originalCredit"].toObject());
    }
    void cachedStableRanking_data(){QTest::addColumn<QJsonObject>("set");for(const auto &v:f::root()["rankSets"].toArray()){const auto set=v.toObject();QTest::newRow(qPrintable(set["id"].toString()))<<set;}}
    void cachedStableRanking(){
        QFETCH(QJsonObject,set);LyricsLookup::resetMatchScoreCount();const auto ranked=LyricsLookup::rankCandidates(f::track(set["track"].toObject()),f::records(set["records"].toArray()));
        QCOMPARE(f::json(ranked),set["expectedOrder"].toArray());QCOMPARE(LyricsLookup::matchScoreCount(),qint64(set["expectedScoreCalls"].toDouble()));
    }
    void largeRankingScoresEachCandidateOnce(){
        const auto rows=f::many();LyricsLookup::resetMatchScoreCount();QElapsedTimer timer;timer.start();const auto ranked=LyricsLookup::rankCandidates(f::performanceTrack(),rows);
        QCOMPARE(ranked.size(),2000);QCOMPARE(LyricsLookup::matchScoreCount(),qint64(2000));
        qInfo().noquote()<<QString("B3 compatibility cached rank n=2000 calls=%1 elapsed_ms=%2 B2_baseline_ms=%3").arg(LyricsLookup::matchScoreCount()).arg(timer.nsecsElapsed()/1000000.).arg(f::root()["performance"].toObject()["milliseconds"].toDouble());
    }
    void compatibilityCallsWorkInWorkers(){
        const auto rows=f::root()["cases"].toArray();LyricsLookup::resetMatchScoreCount();std::vector<std::future<bool>> workers;
        for(int i=0;i<4;++i)workers.push_back(std::async(std::launch::async,[rows]{for(const auto &v:rows){const auto row=v.toObject();if(LyricsLookup::matchScore(f::track(row["track"].toObject()),row["record"].toObject())!=row["expected"].toObject()["score"].toString().toDouble())return false;}return true;}));
        for(auto &worker:workers)QVERIFY(worker.get());QCOMPARE(LyricsLookup::matchScoreCount(),qint64(4*rows.size()));
    }
    void untrustedLyricsAreNotAutomaticallySaved_data(){
        QTest::addColumn<QString>("body");QTest::newRow("html")<<QString("[00:00.00]<div>one</div>\n[00:02.00]two");QTest::newRow("one-line")<<QString("[00:00.00]one");QTest::newRow("beyond-duration")<<QString("[00:00.00]one\n[00:50.00]two");
    }
    void untrustedLyricsAreNotAutomaticallySaved(){
        QFETCH(QString,body);QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        Track t;t.title="策略测试";t.artist="歌手甲";t.album="测试专辑";t.hash="b3-rejection";t.file="Music/b3.wav";t.duration=10000;
        QFile file(library.absolute(t.file));QVERIFY(file.open(QIODevice::WriteOnly));QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(160000);const QByteArray pcm(160000,'\0');out.writeRawData(pcm.constData(),pcm.size());file.close();t.id=library.insert(t);QVERIFY(t.id);
        RejectionServer server;server.candidate={{"trackName",t.title},{"artistName",t.artist},{"albumName",t.album},{"duration",10.0},{"syncedLyrics",body}};QVERIFY(server.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{t.id},nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *list=dialog.findChild<QListWidget *>("lyricsTracks");QVERIFY(list);
        QTRY_VERIFY_WITH_TIMEOUT(list->item(0)->data(Qt::UserRole+2).toString().contains("需选择版本"),10000);
        QVERIFY(server.requests>0);QVERIFY(library.track(t.id).lyrics.isEmpty());dialog.close();
    }
};
QTEST_MAIN(LyricMatchCompatTests)
#include "lyric-match-compat-tests.moc"
