#include "window.h"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QScreen>
#include <QScrollBar>
#include <QSqlError>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QtTest>
#include "lookup-drive.h"
#include <QPainter>
#include <QWheelEvent>
using namespace relaxer;
class FixtureServer:public QTcpServer {
public:
    int requests=0;bool hold=false;QList<QUrl> urls;std::function<bool(const QUrl &)> shouldHold;
    std::function<QPair<int,QByteArray>(const QUrl &)> reply;
    FixtureServer(){connect(this,&QTcpServer::newConnection,this,[this]{auto *socket=nextPendingConnection();socket->setProperty("requestBuffer",QByteArray());connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);connect(socket,&QTcpSocket::readyRead,socket,[this,socket]{auto bytes=socket->property("requestBuffer").toByteArray()+socket->readAll();socket->setProperty("requestBuffer",bytes);if(!bytes.contains("\r\n\r\n")||socket->property("answered").toBool())return;socket->setProperty("answered",true);const auto target=bytes.split(' ').value(1);const QUrl url=QUrl::fromEncoded("http://localhost"+target);urls<<url;++requests;if(hold||(shouldHold&&shouldHold(url)))return;const auto response=reply(url);const auto body=response.second;socket->write("HTTP/1.1 "+QByteArray::number(response.first)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});}
    QUrl endpoint()const{return QUrl(QString("http://127.0.0.1:%1/api").arg(serverPort()));}
};
class V15Tests : public QObject {
    Q_OBJECT
    static Track seed(Library &l,const QString &title,const QString &hash,const QString &artist="测试歌手"){
        Track t;t.title=title;t.hash=hash;t.artist=artist;t.album="原始专辑";t.file="Music/"+hash+".wav";t.duration=10000;t.coverOrigin="none";
        QFile file(l.absolute(t.file));if(!file.open(QIODevice::WriteOnly))return {};
        QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(160000);out.writeRawData(QByteArray(160000,'\0').constData(),160000);file.close();t.id=l.insert(t);return l.track(t.id);
    }
    static void capture(QWidget &w,const QString &name){const auto dir=qEnvironmentVariable("RELAXER_QA_DIR");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.grab().save(dir+"/v15-"+name));}}
    static void wheel(QWidget *target,int delta){
        const QPoint point=target->rect().center();
        QWheelEvent event(point,target->mapToGlobal(point),QPoint(),QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(target,&event);
    }
private slots:
    void initTestCase(){qApp->setStyle("Fusion");}
    void artworkKeepsCircularFeatures(){
        QTemporaryDir dir;QImage source(320,160,QImage::Format_RGB32);source.fill(Qt::black);QPainter p(&source);p.setRenderHint(QPainter::Antialiasing);p.setBrush(Qt::white);p.setPen(Qt::NoPen);p.drawEllipse(QPointF(160,80),20,20);p.end();const auto path=dir.filePath("art.png");QVERIFY(source.save(path));
        Backdrop backdrop;backdrop.setArtwork(path,{},false);backdrop.show();
        for(const auto size:{QSize(600,200),QSize(200,600)}){
            backdrop.resize(size);QTest::qWait(20);const auto image=backdrop.grab().toImage();const int dpr=qRound(image.devicePixelRatio());int horizontal=0,vertical=0;
            for(int x=0;x<image.width();++x)if(image.pixelColor(x,image.height()/2).red()>242)++horizontal;
            for(int y=0;y<image.height();++y)if(image.pixelColor(image.width()/2,y).red()>242)++vertical;
            QVERIFY(horizontal>30*dpr);QVERIFY(qAbs(horizontal-vertical)<=4*dpr);
        }
        capture(backdrop,"aspect-fill.png");
    }
    void tinyWheelAlwaysCompletesProfileMotion(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());for(int i=0;i<24;++i)seed(l,QString("歌曲 %1").arg(i),QString::number(i));
        Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();w.showProfile();QTest::qWait(260);
        auto *header=w.findChild<ProfileHeader *>();auto *table=w.findChild<SongTable *>("recentTable");table->setSongs(l.tracks(),l.root());
        wheel(table->viewport(),-15);QCOMPARE(header->target(),1.);QTest::qWait(230);QCOMPARE(header->height(),80);QCOMPARE(header->collapse(),1.);
        capture(w,"profile-compact.png");
        wheel(table->viewport(),15);QCOMPARE(header->target(),0.);QTest::qWait(230);QCOMPARE(header->height(),192);QCOMPARE(header->collapse(),0.);
        wheel(table->viewport(),-15);QTest::qWait(30);wheel(table->viewport(),15);QTest::qWait(230);QCOMPARE(header->height(),192);
        table->verticalScrollBar()->setValue(1);QTest::qWait(230);QCOMPARE(header->height(),80);table->verticalScrollBar()->setValue(0);QTest::qWait(230);QCOMPARE(header->height(),192);
        capture(w,"profile-expanded.png");
    }
    void filtersUseFixedBlurAndOpaqueSurface(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());for(int i=0;i<20;++i){seed(l,QString("歌曲 %1").arg(i),QString::number(i),QString("歌手 %1").arg(i));}
        for(int theme:{0,1}){
            l.setSetting("theme",theme);Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();QTest::qWait(80);
            for(const auto name:{"artistFilter","albumFilter"}){
                w.findChild<FilterButton *>(name)->click();auto *popup=QApplication::activePopupWidget();QVERIFY(popup);QCOMPARE(popup->property("blurPercent").toInt(),25);
                QTest::qWait(40);const auto image=popup->grab().toImage();const auto pixel=image.pixelColor(image.width()/2,image.height()-10);
                QCOMPARE(pixel.alpha(),255);QVERIFY(qAbs(pixel.lightness()-qApp->palette().color(QPalette::Button).lightness())<55);QVERIFY(image.pixelColor(0,0).alpha()<40);
                capture(*popup,QString(name)+(theme?"-dark.png":"-light.png"));popup->close();
            }
        }
    }
    void lyricsRecommendPreserveChoiceAndFilterCompleted(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"已有歌词","a"),b=seed(l,"版本选择","b"),c=seed(l,"等待歌曲","c");
        QVERIFY(l.saveLyrics(a.id,"[00:00.00]本地歌词"));
        QJsonObject near{{"trackName",b.title},{"artistName",b.artist},{"albumName","不同专辑"},{"duration",15.},{"syncedLyrics","[00:00.00]recommended"},{"plainLyrics","recommended"}};
        auto far=near;far["duration"]=60.;far["syncedLyrics"]="[00:00.00]far";
        QVERIFY(LyricsLookup::matchScore(b,near)>LyricsLookup::matchScore(b,far));QVERIFY(!LyricsLookup::confident(b,near));
        FixtureServer server;QVERIFY(server.listen(QHostAddress::LocalHost));server.shouldHold=[&](const QUrl &u){const QUrlQuery q(u);return q.queryItemValue("track_name")==c.title||q.queryItemValue("q")==c.title;};server.reply=[&](const QUrl &url){
            if(url.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));
            return qMakePair(200,QJsonDocument(QJsonArray{far,near}).toJson());
        };
        Window parent(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))parent.playback()->media()->setAudioOutput(nullptr);parent.show();LyricsLookup dialog(&l,{a.id,b.id,c.id},&parent,server.endpoint());dialog.show();relaxerClickSearch(&dialog);
        auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");auto *versions=dialog.findChild<QListWidget *>("lyricsVersions");auto *filter=dialog.findChild<QPushButton *>("lyricsIncomplete");
        const auto rowOf=[&](qint64 id){for(int r=0;r<tasks->count();++r)if(tasks->item(r)->data(Qt::UserRole).toLongLong()==id)return r;return -1;};const int rowA=rowOf(a.id),rowB=rowOf(b.id),rowC=rowOf(c.id);QVERIFY(rowA>=0&&rowB>=0&&rowC>=0);
        QVERIFY(dialog.findChild<QCheckBox *>("lyricsMissingOnly")->isChecked());QVERIFY(tasks->item(rowA)->isHidden());
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(rowB)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);
        QCOMPARE(tasks->currentRow(),-1);QCOMPARE(versions->count(),0);QVERIFY(dialog.findChild<QPlainTextEdit *>("lyricsPreview")->isHidden());capture(dialog,"lyrics-blank.png");
        tasks->setCurrentRow(rowB);QCOMPARE(versions->count(),2);QVERIFY(versions->item(0)->text().startsWith("★ 最推荐"));QVERIFY(dialog.findChild<QPlainTextEdit *>("lyricsPreview")->toPlainText().contains("recommended"));
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(rowC)->data(Qt::UserRole+2).toString().contains("正在"),5000);
        QCOMPARE(tasks->currentRow(),rowB);const int angle=tasks->property("spinnerAngle").toInt();QTest::qWait(100);QVERIFY(tasks->property("spinnerAngle").toInt()!=angle);capture(dialog,"lyrics-candidates.png");
        dialog.findChild<QPushButton *>("lyricsStop")->click();QVERIFY(tasks->item(rowC)->data(Qt::UserRole+2).toString().startsWith("已停止"));
        dialog.findChild<QPushButton *>("lyricsStart")->click();QTRY_VERIFY_WITH_TIMEOUT(tasks->item(rowB)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);QVERIFY(tasks->item(rowC)->data(Qt::UserRole+2).toString().startsWith("已停止"));QCOMPARE(tasks->currentRow(),rowB);
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);QVERIFY(!tasks->item(rowA)->isHidden());QCOMPARE(tasks->item(rowA)->data(Qt::UserRole+1).toInt(),2);
        filter->click();QVERIFY(filter->isChecked());QVERIFY(tasks->item(rowA)->isHidden());QVERIFY(!tasks->item(rowB)->isHidden());
        dialog.findChild<QPushButton *>("lyricsApply")->click();QVERIFY(!l.track(b.id).lyrics.isEmpty());QVERIFY(tasks->item(rowB)->isHidden());QCOMPARE(versions->count(),0);QVERIFY(!dialog.findChild<QPushButton *>("lyricsApply")->isEnabled());
        QFile saved(l.absolute(l.track(b.id).lyrics));QVERIFY(saved.open(QIODevice::ReadOnly));QVERIFY(saved.readAll().contains("recommended"));
        filter->click();QVERIFY(!filter->isChecked());QVERIFY(!tasks->item(rowA)->isHidden());QVERIFY(!tasks->item(rowB)->isHidden());
        dialog.findChild<QPushButton *>("lyricsSaveClose")->click();QVERIFY(!dialog.isVisible());
    }
    void saveCloseAppliesSelectedVersion(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto track=seed(l,"确认版本","track");FixtureServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        QJsonObject record{{"trackName",track.title},{"artistName",track.artist},{"albumName","候选专辑"},{"duration",15.},{"plainLyrics","Save and close fixture"}};
        server.reply=[&](const QUrl &url){return url.path().endsWith("/get")?qMakePair(404,QByteArray("{}")):qMakePair(200,QJsonDocument(QJsonArray{record}).toJson());};
        LyricsLookup dialog(&l,{track.id},nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);tasks->setCurrentRow(0);
        dialog.findChild<QPushButton *>("lyricsSaveClose")->click();QVERIFY(!l.track(track.id).lyrics.isEmpty());QVERIFY(!dialog.isVisible());
    }
};
QTEST_MAIN(V15Tests)
#include "v15-tests.moc"
