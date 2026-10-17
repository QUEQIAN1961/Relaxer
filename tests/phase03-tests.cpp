#include "lyricslookup.h"
#include "lyrics.h"
#include <algorithm>
#include <QApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>
#include "lookup-drive.h"
using namespace relaxer;
class Phase03Server:public QTcpServer {
public:
    int requests=0;bool hold=false;QList<QUrl> urls;QList<QPointer<QTcpSocket>> held;
    std::function<QPair<int,QByteArray>(const QUrl &)> reply=[](const QUrl &){return qMakePair(404,QByteArray("{}"));};
    Phase03Server(){connect(this,&QTcpServer::newConnection,this,[this]{
        auto *s=nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
        connect(s,&QTcpSocket::readyRead,s,[this,s]{
            auto b=s->property("buffer").toByteArray()+s->readAll();s->setProperty("buffer",b);
            if(!b.contains("\r\n\r\n")||s->property("answered").toBool())return;s->setProperty("answered",true);
            if(!b.contains("User-Agent: Relaxer/")){s->disconnectFromHost();return;}
            auto u=QUrl::fromEncoded("http://localhost"+b.split(' ').value(1));urls<<u;++requests;
            if(hold){held<<s;return;}auto r=reply(u);send(s,r.first,r.second);
        });
    });}
    static void send(QTcpSocket *s,int code,const QByteArray &body){if(!s)return;s->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();}
    QUrl endpoint()const{return QUrl(QString("http://127.0.0.1:%1/api").arg(serverPort()));}
};
class Phase03Tests:public QObject {
    Q_OBJECT
    static Track seed(Library &l,QString name,QString hash,QString artist="歌手甲"){
        Track t;t.title=name;t.hash=hash;t.artist=artist;t.album="测试专辑";t.file="Music/"+hash+".wav";t.duration=10000;t.coverOrigin="none";
        QFile f(l.absolute(t.file));if(!f.open(QIODevice::WriteOnly))return {};
        QDataStream out(&f);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(160000);out.writeRawData(QByteArray(160000,'\0').constData(),160000);f.close();t.id=l.insert(t);return l.track(t.id);
    }
    static QJsonObject record(QString title,QString artist,double seconds,QString line="第一行歌词",QString line2="第二行歌词"){
        const auto plain=line+"\n"+line2;const auto synced="[00:00.00]"+line+"\n[00:02.00]"+line2;
        return {{"trackName",title},{"artistName",artist},{"albumName","测试专辑"},{"duration",seconds},{"syncedLyrics",synced},{"plainLyrics",plain}};
    }
private slots:
    void selectedCandidateIdentitySurvivesLateReordering(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"Selection stable","stable-id","Original artist");
        Phase03Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.hold=true;
        LyricsLookup dialog(&l,{t.id},nullptr,server.endpoint());dialog.show();auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");
        tracks->setCurrentRow(0);relaxerClickSearch(&dialog);QTRY_VERIFY(server.held.size()>=3);
        QPointer<QTcpSocket> firstSearch;for(const auto &socket:server.held)if(socket&&socket->property("buffer").toByteArray().contains("/search")){firstSearch=socket;break;}
        QVERIFY(firstSearch);auto a=record(t.title,"Other A",30,"A1","A2"),b=record(t.title,"Other B",40,"B1","B2");a["id"]=1;b["id"]=2;
        Phase03Server::send(firstSearch,200,QJsonDocument(QJsonArray{a,b}).toJson());server.held.removeAll(firstSearch);
        auto *versions=dialog.findChild<QListWidget *>("lyricsVersions");QTRY_COMPARE(versions->count(),2);versions->setCurrentRow(1);
        const auto selected=versions->currentItem()->data(Qt::UserRole+5).toString();QVERIFY(!selected.isEmpty());
        auto better=record(t.title,t.artist,10,"New1","New2");better["id"]=3;better["manualOnly"]=true;
        const auto held=server.held;server.held.clear();server.hold=false;
        server.reply=[](const QUrl &u){return qMakePair(u.path().endsWith("/get")?404:200,u.path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));};
        for(const auto &socket:held)if(socket){const bool search=socket->property("buffer").toByteArray().contains("/search");Phase03Server::send(socket,search?200:404,search?QJsonDocument(QJsonArray{better}).toJson():QByteArray("{}"));}
        QTRY_COMPARE(versions->count(),3);QCOMPARE(versions->currentItem()->data(Qt::UserRole+5).toString(),selected);
        QTRY_VERIFY(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled());QCOMPARE(versions->currentItem()->data(Qt::UserRole+5).toString(),selected);
        QVERIFY(l.track(t.id).lyrics.isEmpty());dialog.close();
    }
    void initTestCase(){qApp->setStyle("Fusion");}
    void rankMatchesComparatorAndCountsEachCandidateOnce(){
        Track query;query.title="夜曲";query.artist="周杰伦";query.album="十一月的萧邦";query.duration=220000;
        QVector<QJsonObject> rows;
        for(int i=0;i<40;++i)rows<<record(i%5==0?"夜曲":QString("其他%1").arg(i),i%7==0?"周杰伦":QString("歌手%1").arg(i),200+i,QString("甲%1").arg(i),QString("乙%1").arg(i));
        rows<<record("夜曲","周杰伦",220,"平分甲","平分乙");rows<<record("夜曲","周杰伦",220,"平分丙","平分丁");
        LyricsLookup::resetMatchScoreCount();QElapsedTimer fast;fast.start();
        const auto ranked=LyricsLookup::rankCandidates(query,rows);const auto fastMs=fast.elapsed();const auto fastCalls=LyricsLookup::matchScoreCount();
        QCOMPARE(fastCalls,qint64(rows.size()));
        LyricsLookup::resetMatchScoreCount();QElapsedTimer slow;slow.start();
        auto expected=rows;std::stable_sort(expected.begin(),expected.end(),[&](const QJsonObject &a,const QJsonObject &b){return LyricsLookup::matchScore(query,a)>LyricsLookup::matchScore(query,b);});
        const auto slowMs=slow.elapsed();const auto slowCalls=LyricsLookup::matchScoreCount();
        QJsonArray rankedJson,expectedJson;for(const auto &r:ranked)rankedJson.append(r);for(const auto &r:expected)expectedJson.append(r);
        QCOMPARE(rankedJson,expectedJson);
        QVERIFY(slowCalls>fastCalls);
        const int tie0=0;QCOMPARE(ranked.at(tie0)["plainLyrics"].toString(),QString("平分甲\n平分乙"));QCOMPARE(ranked.at(1)["plainLyrics"].toString(),QString("平分丙\n平分丁"));
        qInfo().noquote()<<QString("phase03 rank n=%1 precompute_ms=%2 calls=%3 comparator_ms=%4 calls=%5 threads=%6").arg(rows.size()).arg(fastMs).arg(fastCalls).arg(slowMs).arg(slowCalls).arg(LyricsLookup::lyricComputeThreadCount());
        const int cores=QThread::idealThreadCount();
        QCOMPARE(LyricsLookup::lyricComputeThreadCount(),std::max(2,cores<1?2:cores));
    }
    void originalAndMatchAreNotInterchangeable(){
        Track original;original.title="歌A";original.artist="甲";original.album="专辑";original.duration=10000;
        Track match=original;match.title="歌B";
        const auto a=record("歌A","甲",10);const auto b=record("歌B","甲",10);
        QCOMPARE(LyricsLookup::rankCandidates(original,{a,b}).first()["trackName"].toString(),QString("歌A"));
        QCOMPARE(LyricsLookup::rankCandidates(match,{a,b}).first()["trackName"].toString(),QString("歌B"));
    }
    void autosaveStillUsesConfidentLyrics(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"测试歌曲","auto");
        Phase03Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[&](const QUrl &u){if(u.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));return qMakePair(200,QJsonDocument(QJsonArray{record("测试歌曲","歌手甲",10)}).toJson());};
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(!l.track(t.id).lyrics.isEmpty(),8000);
        QFile saved(l.absolute(l.track(t.id).lyrics));QVERIFY(saved.open(QIODevice::ReadOnly));QVERIFY(saved.readAll().contains("第一行歌词"));
        dialog.close();
    }
    void earlyStopLeavesLaterAttemptsUnsent(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"测试歌曲","early");
        Phase03Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[&](const QUrl &u){if(u.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));return qMakePair(200,QJsonDocument(QJsonArray{record("测试歌曲","歌手甲",10)}).toJson());};
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(!l.track(t.id).lyrics.isEmpty(),8000);
        const int sent=s.requests;QTest::qWait(500);QCOMPARE(s.requests,sent);QVERIFY(sent<12);
        dialog.close();
    }
    void newerCandidatesReplaceStaleRanking(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"不会自动保存","stale","歌手甲");
        Phase03Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.hold=true;
        LyricsLookup::resetLyricRankRuns();
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.setProperty("lyricScoreDelayMs",400);dialog.setProperty("lyricsRequestTimeoutMs",400);dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(s.held.size()>=2,8000);
        int sent=0;
        for(int i=0;i<s.held.size();++i){const bool search=s.urls.value(i).path().endsWith("/search");if(search&&sent<2){auto r=record(sent==0?"甲候选":"乙候选",sent==0?"别人一":"别人二",30+sent,sent==0?"甲一":"乙一",sent==0?"甲二":"乙二");Phase03Server::send(s.held[i],200,QJsonDocument(QJsonArray{r}).toJson());++sent;}else Phase03Server::send(s.held[i],s.urls.value(i).path().endsWith("/get")?404:200,s.urls.value(i).path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));}
        s.hold=false;s.reply=[](const QUrl &u){return qMakePair(u.path().endsWith("/get")?404:200,u.path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));};
        auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().contains("2 个"),15000);
        QVERIFY(l.track(t.id).lyrics.isEmpty());QVERIFY(LyricsLookup::lyricRankRuns()>=2);
        dialog.close();
    }
    void stopAndCloseDropInflightResults(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"测试歌曲","cancel");
        Phase03Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[&](const QUrl &u){if(u.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));return qMakePair(200,QJsonDocument(QJsonArray{record("测试歌曲","歌手甲",10)}).toJson());};
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.setProperty("lyricScoreDelayMs",1500);dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(s.requests>=1,8000);
        dialog.findChild<QPushButton *>("lyricsStop")->click();
        QTest::qWait(1800);QVERIFY(l.track(t.id).lyrics.isEmpty());
        auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");QVERIFY(tasks->item(0)->data(Qt::UserRole+2).toString().contains("已停止"));
        dialog.close();
        const int before=s.requests;
        LyricsLookup again(&l,{t.id},nullptr,s.endpoint());again.setProperty("lyricScoreDelayMs",1500);again.show();relaxerClickSearch(&again);
        QTRY_VERIFY_WITH_TIMEOUT(s.requests>before,8000);again.close();QTest::qWait(1800);QVERIFY(l.track(t.id).lyrics.isEmpty());
    }
    void eventLoopStaysResponsiveDuringRanking(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"测试歌曲","gui");
        Phase03Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[&](const QUrl &u){if(u.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));return qMakePair(200,QJsonDocument(QJsonArray{record("测试歌曲","歌手甲",10)}).toJson());};
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.setProperty("lyricScoreDelayMs",800);dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(s.requests>=1,8000);
        int ticks=0;QTimer timer;timer.setInterval(40);connect(&timer,&QTimer::timeout,[&]{++ticks;});timer.start();
        QElapsedTimer wall;wall.start();QTest::qWait(400);timer.stop();
        QVERIFY2(wall.elapsed()<700,"GUI thread blocked inside the ranking wait");
        QVERIFY2(ticks>=5,"event loop did not keep dispatching timers");
        dialog.findChild<QPushButton *>("lyricsStop")->click();dialog.close();
    }
};
QTEST_MAIN(Phase03Tests)
#include "phase03-tests.moc"
