#include "window.h"
#include <QStatusBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include "lyrics.h"
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QStringConverter>
#include <QFontMetrics>
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QJsonDocument>
#include <QHeaderView>
#include <QPainter>
#include <QScrollArea>
#include <QSqlQuery>
#include <QEventLoop>
#include <QNetworkProxyFactory>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QtTest>
#include "lookup-drive.h"
using namespace relaxer;
class V16Server:public QTcpServer {
public:
    int requests=0;bool hold=false;QList<QUrl> urls;QList<QPointer<QTcpSocket>> held;
    std::function<QPair<int,QByteArray>(const QUrl &)> reply;
    V16Server(){connect(this,&QTcpServer::newConnection,this,[this]{
        auto *s=nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
        connect(s,&QTcpSocket::readyRead,s,[this,s]{
            auto b=s->property("buffer").toByteArray()+s->readAll();s->setProperty("buffer",b);
            if(!b.contains("\r\n\r\n")||s->property("answered").toBool())return;s->setProperty("answered",true);
            if(!b.contains("User-Agent: Relaxer/")){s->disconnectFromHost();return;}
            auto u=QUrl::fromEncoded("http://localhost"+b.split(' ').value(1));urls<<u;++requests;
            if(hold){held<<s;return;}auto r=reply(u);send(s,r.first,r.second);
        });
    });}
    static void send(QTcpSocket *s,int code,const QByteArray &body){s->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();}
    QUrl endpoint(QString path="/api")const{return QUrl(QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));}
};
class V16Tests:public QObject {
    Q_OBJECT
    static Track seed(Library &l,QString name="测试歌曲",QString hash="seed",QString artist="歌手甲") {
        Track t;t.title=name;t.hash=hash;t.artist=artist;t.album="测试专辑";t.file="Music/"+hash+".wav";t.duration=10000;t.coverOrigin="none";
        QFile f(l.absolute(t.file));if(!f.open(QIODevice::WriteOnly))return {};
        QDataStream out(&f);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(160000);out.writeRawData(QByteArray(160000,'\0').constData(),160000);f.close();t.id=l.insert(t);return l.track(t.id);
    }
    static QJsonObject record(QString title,QString artist="歌手甲",double seconds=10.) {
        return {{"trackName",title},{"artistName",artist},{"albumName","测试专辑"},{"duration",seconds},{"syncedLyrics","[00:00.00]原创验证第一行\n[00:02.00]原创验证第二行"},{"plainLyrics","原创验证第一行\n原创验证第二行"}};
    }
    static void capture(QWidget &w,QString file){const auto dir=qEnvironmentVariable("RELAXER_QA_DIR");if(!dir.isEmpty()){QDir().mkpath(dir);w.grab().save(dir+"/"+file);}}
    static void wheel(QWidget *target,int delta){QWheelEvent event(QPointF(20,20),target->mapToGlobal(QPoint(20,20)),QPoint(),QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(target,&event);}
private slots:
    void initTestCase(){qApp->setStyle("Fusion");}
    void cleaningAndRecordingSafety(){
        QCOMPARE(LyricsLookup::cleanTitle("01. 7 Years (Live Remix) [FLAC].mp3"),QString("7 Years (Live Remix)"));
        QCOMPARE(LyricsLookup::cleanTitle("7 Years"),QString("7 Years"));QCOMPARE(LyricsLookup::cleanTitle("4.44"),QString("4.44"));QCOMPARE(LyricsLookup::cleanTitle("24-7"),QString("24-7"));
        Track t;t.title="03. 测试歌曲 [320kbps]";t.artist="歌手甲";t.duration=10000;t.album="导入标签专辑";
        QVERIFY(LyricsLookup::confident(t,record("测试歌曲")));
        QVERIFY(!LyricsLookup::confident(t,record("测试歌曲","歌手乙")));
        QVERIFY(!LyricsLookup::confident(t,record("测试歌曲","歌手甲",30)));
        t.title="测试歌曲 (Live)";QVERIFY(!LyricsLookup::confident(t,record("测试歌曲")));
        QVERIFY(LyricsLookup::confident(t,record("测试歌曲 (Live)")));
        t.title="测试歌曲";t.artist="A/B";QVERIFY(!LyricsLookup::confident(t,record("测试歌曲","A")));
        QVERIFY(LyricsLookup::confident(t,record("测试歌曲","B & A")));
        t.artist="FrogMonster 蛙蛙";QVERIFY(LyricsLookup::confident(t,record("测试歌曲","FrogMonster")));
        t.artist="未知歌手";QVERIFY(!LyricsLookup::confident(t,record("测试歌曲","歌手甲")));
    }
    void nonmatchingResultsContinueToRelaxedSearch(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=seed(l,"01. 测试歌曲 [FLAC]","a");
        V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[&](const QUrl &u){
            if(u.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));
            const auto q=QUrlQuery(u);
            auto r=record("测试歌曲",q.hasQueryItem("artist_name")?"歌手乙":"歌手甲");
            return qMakePair(200,QJsonDocument(QJsonArray{r}).toJson());
        };
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(!l.track(t.id).lyrics.isEmpty(),5000);
        bool withArtist=false,titleOnly=false;for(const auto &u:s.urls)if(u.path().endsWith("/search")){if(QUrlQuery(u).hasQueryItem("artist_name"))withArtist=true;else titleOnly=true;}
        QVERIFY(withArtist);QVERIFY(titleOnly);qInfo()<<"D-R7-03 relaxed fallback request count"<<s.requests;
        dialog.close();
        const int before=s.requests;LyricsLookup cached(&l,{t.id},nullptr,s.endpoint());cached.show();cached.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);relaxerClickSearch(&cached);auto *cachedTasks=cached.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(cachedTasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);QTRY_VERIFY_WITH_TIMEOUT(s.requests>before,5000);cached.close();
    }
    void manualSearchNeverChangesLibraryMetadata(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=seed(l,"错误标签","m");
        V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[&](const QUrl &u){
            if(QUrlQuery(u).queryItemValue("track_name")=="修正歌名")return qMakePair(200,QJsonDocument(u.path().endsWith("/get")?QJsonDocument(record("修正歌名")):QJsonDocument(QJsonArray{record("修正歌名")})).toJson());
            return qMakePair(u.path().endsWith("/get")?404:200,u.path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));
        };
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().contains("数据库无结果"),5000);tasks->setCurrentRow(0);
        dialog.findChild<QLineEdit *>("lyricsSearchTitle")->setText("修正歌名");dialog.findChild<QPushButton *>("lyricsManualSearch")->click();
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);
        QVERIFY(l.track(t.id).lyrics.isEmpty());QCOMPARE(l.track(t.id).title,t.title);
        auto *versions=dialog.findChild<QListWidget *>("lyricsVersions");QVERIFY(versions->count()>0);QVERIFY(versions->item(0)->text().contains("LRCLIB"));
        dialog.findChild<QPushButton *>("lyricsApply")->click();QVERIFY(!l.track(t.id).lyrics.isEmpty());QCOMPARE(l.track(t.id).title,t.title);capture(dialog,"v16-manual-lyrics.png");dialog.close();
    }
    void backupRequiresManualConfirmation(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=seed(l,"备用歌曲","b");
        V16Server s,b;QVERIFY(s.listen(QHostAddress::LocalHost));QVERIFY(b.listen(QHostAddress::LocalHost));
        s.reply=[](const QUrl &u){return qMakePair(u.path().endsWith("/get")?404:200,u.path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));};
        b.reply=[](const QUrl &){return qMakePair(200,QByteArray("{\"lyrics\":\"Independent original fallback fixture\"}"));};
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint(),b.endpoint("/v1"));dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);QVERIFY(b.requests>0);QVERIFY(l.track(t.id).lyrics.isEmpty());
        tasks->setCurrentRow(0);auto *versions=dialog.findChild<QListWidget *>("lyricsVersions");QVERIFY(versions->item(0)->text().contains("Lyrics.ovh"));
        dialog.findChild<QPushButton *>("lyricsApply")->click();QVERIFY(!l.track(t.id).lyrics.isEmpty());dialog.close();
    }
    void timeoutAndCancelAreDistinctFromNoResults(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=seed(l,"超时测试","timeout");
        V16Server s;s.hold=true;QVERIFY(s.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.setProperty("lyricsRequestTimeoutMs",100);dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");
        QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().contains("请求超时"),5000);QVERIFY(l.track(t.id).lyrics.isEmpty());dialog.close();
        LyricsLookup cancel(&l,{t.id},nullptr,s.endpoint());cancel.show();relaxerClickSearch(&cancel);const int before=s.requests;QTRY_VERIFY(s.requests>before);
        cancel.findChild<QPushButton *>("lyricsStop")->click();for(auto socket:s.held)if(socket)V16Server::send(socket,200,QJsonDocument(record(t.title)).toJson());
        QTest::qWait(200);QVERIFY(l.track(t.id).lyrics.isEmpty());QVERIFY(cancel.findChild<QPushButton *>("lyricsStart")->isEnabled());cancel.close();
    }
    void switchingTracksCannotApplyTheOtherLyrics(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=seed(l,"切歌甲","a"),b=seed(l,"切歌乙","b");
        // Restore paused tracks so this response-identity test does not require a physical audio device.
        l.setSetting("queue",QJsonArray{double(a.id),double(b.id)});l.setSetting("current",double(a.id));
        Window w(&l);w.show();w.playback()->media()->setAudioOutput(nullptr);QCOMPARE(w.playback()->current().id,a.id);
        V16Server s;s.hold=true;QVERIFY(s.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&l,{a.id},nullptr,s.endpoint());dialog.setProperty("lyricsRequestTimeoutMs",20000);dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY(s.requests>0);
        l.setSetting("current",double(b.id));w.playback()->restore();w.playback()->media()->setAudioOutput(nullptr);QCOMPARE(w.playback()->current().id,b.id);
        const auto answer=QJsonDocument(record(a.title)).toJson();
        QElapsedTimer wait;wait.start();bool settled=false;
        while(wait.elapsed()<8000){
            if(!s.held.isEmpty()){const auto pending=s.held;s.held.clear();for(auto socket:pending)if(socket)V16Server::send(socket,200,answer);}
            const auto text=dialog.findChild<QListWidget *>("lyricsTracks")->item(0)->data(Qt::UserRole+2).toString();
            if(!text.contains("正在")&&!text.startsWith("等待")){settled=true;break;}
            QTest::qWait(20);
        }
        QVERIFY(settled);
        QVERIFY(!l.track(a.id).lyrics.isEmpty());QCOMPARE(w.playback()->current().id,b.id);QVERIFY(l.track(b.id).lyrics.isEmpty());dialog.close();
    }
    void playlistCompactLayoutAndReversal(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());for(int i=0;i<30;++i)seed(l,QString("歌曲 %1").arg(i),QString::number(i));
        auto id=l.createPlaylist("动画验证");QVERIFY(l.addToPlaylist(id,l.tracks().isEmpty()?QVector<qint64>{}:QVector<qint64>{l.tracks()[0].id}));
        QVector<qint64> ids;for(const auto &t:l.tracks())ids<<t.id;QVERIFY(l.addToPlaylist(id,ids));QVERIFY(l.playlistDescription(id,QString("简介内容用于验证右侧单行省略和按钮间距。").repeated(20)));
        Window w(&l);w.resize(1240,790);w.show();QTest::qWait(70);auto *nav=w.findChild<QListWidget *>("playlistNavigation");nav->setCurrentRow(0);QMetaObject::invokeMethod(nav,"itemClicked",Qt::DirectConnection,Q_ARG(QListWidgetItem *,nav->item(0)));QTest::qWait(260);
        auto *h=w.findChild<PlaylistHeader *>();QVERIFY(h->isVisible());auto *table=w.findChild<SongTable *>("songTable");
        wheel(table->viewport(),-1);QTRY_COMPARE(h->height(),78);
        auto *row1=h->findChild<QWidget *>("playlistRow0"),*row2=h->findChild<QWidget *>("playlistRow1"),*row3=h->findChild<QWidget *>("playlistRow2"),*row4=h->findChild<QWidget *>("playlistRow3");
        QCOMPARE(row1->y(),row3->y());QCOMPARE(row2->y(),row4->y());QCOMPARE(h->cover()->geometry().bottom(),row2->geometry().bottom());
        auto *description=h->findChild<DescriptionPreview *>();QCOMPARE(description->displayLines().size(),1);QVERIFY(description->isElided());
        auto changed=l.playlists().front();changed.description="短简介";h->setPlaylist(changed,l.root());QCOMPARE(description->mapTo(h,QPoint(description->width(),0)).x(),h->width());QVERIFY(description->mapTo(h,QPoint()).x()>h->findChild<QPushButton *>("playlistShuffle")->mapTo(h,QPoint(h->findChild<QPushButton *>("playlistShuffle")->width(),0)).x()+20);
        auto *shuffle=h->findChild<QPushButton *>("playlistShuffle");QVERIFY(description->mapTo(h,QPoint()).x()>shuffle->mapTo(h,QPoint(shuffle->width(),0)).x()+20);
        capture(w,"v16-playlist-compact.png");h->collapseTo(0);QTest::qWait(40);h->collapseTo(1);QTRY_COMPARE(h->height(),78);
        w.resize(1050,700);QTest::qWait(30);QVERIFY(description->mapTo(h,QPoint()).x()>shuffle->mapTo(h,QPoint(shuffle->width(),0)).x()+20);
        h->collapseTo(0);QTRY_COMPARE(h->height(),160);capture(w,"v16-playlist-expanded.png");
    }
    void interactiveWidthsAreRememberedAndIconsFixed(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());
        {SongTable table;table.setColumnLibrary(&l);table.resize(900,250);table.show();QTest::qWait(30);
        QCOMPARE(table.horizontalHeader()->sectionResizeMode(SongTable::NumberColumn),QHeaderView::Fixed);QCOMPARE(table.horizontalHeader()->sectionResizeMode(SongTable::FavoriteColumn),QHeaderView::Fixed);
        QCOMPARE(table.horizontalHeader()->sectionResizeMode(SongTable::ArtistColumn),QHeaderView::Interactive);table.horizontalHeader()->resizeSection(SongTable::ArtistColumn,205);QTest::qWait(220);
        QCOMPARE(l.setting("songColumnWidths").toObject()["artist"].toInt(),205);table.setColumnsMode(SongTable::LocalMetadata);QCOMPARE(table.columnWidth(SongTable::ArtistColumn),205);table.horizontalHeader()->resizeSection(SongTable::AlbumColumn,185);}
        SongTable again;again.setColumnLibrary(&l);QCOMPARE(again.columnWidth(SongTable::ArtistColumn),205);QCOMPARE(again.columnWidth(SongTable::AlbumColumn),185);
    }
    void profileSelectionAndCleanTooltips(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());seed(l);
        Window w(&l);w.show();w.showProfile();QTest::qWait(260);auto *recent=w.findChild<QPushButton *>("profileRecent"),*frequent=w.findChild<QPushButton *>("profileFrequent");QVERIFY(recent->isChecked());QVERIFY(!frequent->isChecked());
        frequent->click();QVERIFY(frequent->isChecked());QVERIFY(!recent->isChecked());recent->click();QVERIFY(recent->isChecked());QVERIFY(!frequent->isChecked());
        auto *table=w.findChild<SongTable *>("songTable");QVERIFY(table->item(0,SongTable::NumberColumn)->toolTip().isEmpty());QVERIFY(table->item(0,SongTable::DurationColumn)->toolTip().isEmpty());QVERIFY(!table->item(0,SongTable::FavoriteColumn)->toolTip().isEmpty());capture(w,"v16-profile.png");
    }
    void stationaryBackgroundDuringOrdinaryMotion(){
        QWidget parent;parent.resize(240,160);parent.show();PageMotion motion(&parent);
        QPixmap bg(240,160);bg.fill(QColor("#324c65"));QPixmap old(240,160),next(240,160);old.fill(Qt::transparent);next.fill(Qt::transparent);
        QPainter p(&old);p.fillRect(70,50,60,30,Qt::red);p.end();QPainter q(&next);q.fillRect(90,60,60,30,Qt::green);q.end();
        motion.hold(parent.rect(),old,bg);motion.startPage(next,bg,[]{});QTest::qWait(35);
        QCOMPARE(motion.currentPage().toImage().pixelColor(5,140),QColor("#324c65"));QCOMPARE(motion.currentContent().toImage().pixelColor(5,140).alpha(),0);
        QTest::qWait(70);QCOMPARE(motion.currentPage().toImage().pixelColor(5,140),QColor("#324c65"));motion.stop();
    }
    void fractionalBlurAndTransparentBodies(){
        QImage image(320,240,QImage::Format_ARGB32);for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)image.setPixel(x,y,(x/12+y/12)%2?qRgb(0,0,0):qRgb(255,255,255));
        QCOMPARE(gaussianBlur(image,0),image);QVERIFY(blurRadius(2.5)<blurRadius(5));QVERIFY(blurRadius(5)<blurRadius(10));QVERIFY(blurRadius(10)<3.4);QCOMPARE(blurRadius(50),17.);
        auto low=gaussianBlur(image,2.5),high=gaussianBlur(image,20);QVERIFY(low!=image);QVERIFY(low!=high);
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());QImage qaImage(1200,900,QImage::Format_RGB32);for(int y=0;y<qaImage.height();++y)for(int x=0;x<qaImage.width();++x)qaImage.setPixelColor(x,y,QColor(230-x*65/1200,185-y*35/900,180+x*35/1200));QVERIFY(qaImage.save(d.filePath("background.png")));auto theme=AppearanceStore::defaults();theme["name"]="验证外观";theme["mode"]="image";theme["sourceLeft"]=d.filePath("background.png");theme["sourceRight"]=d.filePath("background.png");theme["shareLR"]=true;theme["blurLeft"]=2.5;theme["blurRight"]=2.5;QVERIFY(AppearanceStore(&l).save(theme,{}));AppearancePage dialog(&l,[](const QJsonObject &){},[]{});auto *spin=dialog.findChild<BlurPercentSpin *>("appearanceBlurValueRight");QVERIFY(spin);QCOMPARE(spin->decimals(),1);spin->setValue(2.5);QCOMPARE(spin->value(),2.5);
        Window w(&l);w.show();w.showSettings();QTest::qWait(260);for(auto *area:w.findChildren<QScrollArea *>())if(area->widget())QVERIFY(!area->widget()->autoFillBackground());capture(w,"v16-settings.png");w.showVersionLog();QTest::qWait(260);capture(w,"v16-log.png");
    }
    void v17CachedCandidatesSurviveReopening(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"候选缓存","cache");
        V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));auto r=record(t.title,t.artist,16.);
        s.reply=[&](const QUrl &u){return u.path().endsWith("/get")?qMakePair(404,QByteArray("{}")):qMakePair(200,QJsonDocument(QJsonArray{r}).toJson());};
        {LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);tasks->setCurrentRow(0);QCOMPARE(dialog.findChild<QListWidget *>("lyricsVersions")->count(),1);QVERIFY(l.track(t.id).lyrics.isEmpty());dialog.close();}
        const int before=s.requests;LyricsLookup cached(&l,{t.id},nullptr,s.endpoint());cached.show();relaxerClickSearch(&cached);auto *tasks=cached.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);tasks->setCurrentRow(0);QCOMPARE(cached.findChild<QListWidget *>("lyricsVersions")->count(),1);QTRY_VERIFY_WITH_TIMEOUT(s.requests>before,5000);cached.close();
    }
    void v17ReuseAndManualOffset(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"同曲复用","reuse-a"),b=seed(l,"同曲复用","reuse-b");const QByteArray data="[00:00.00]first\n[00:02.00]second";QVERIFY(l.saveLyrics(a.id,data));
        V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[](const QUrl &){return qMakePair(200,QByteArray("[]"));};
        LyricsLookup copy(&l,{b.id},nullptr,s.endpoint());copy.show();relaxerClickSearch(&copy);auto *tasks=copy.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("已保存同曲歌词"));QCOMPARE(s.requests,0);QFile saved(l.absolute(l.track(b.id).lyrics));QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),data);copy.close();
        const auto c=seed(l,"手动偏移","offset");auto r=record(c.title,c.artist,16.);s.reply=[&](const QUrl &u){return u.path().endsWith("/get")?qMakePair(404,QByteArray("{}")):qMakePair(200,QJsonDocument(QJsonArray{r}).toJson());};LyricsLookup offset(&l,{c.id},nullptr,s.endpoint());offset.show();relaxerClickSearch(&offset);auto *rows=offset.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(rows->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);rows->setCurrentRow(0);offset.findChild<QSpinBox *>("lyricsOffset")->setValue(800);offset.findChild<QPushButton *>("lyricsApply")->click();QFile file(l.absolute(l.track(c.id).lyrics));QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();QVERIFY(bytes.contains("[offset:800]"));QCOMPARE(Lyrics::parse(bytes).lines.first().time,qint64(800));offset.close();
    }
    void v17SourceFilteringUsesRecordMapping(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"筛选映射","filter");V16Server s,b;QVERIFY(s.listen(QHostAddress::LocalHost));QVERIFY(b.listen(QHostAddress::LocalHost));auto synced=record(t.title,t.artist,16.),plain=synced;plain["syncedLyrics"]="";plain["plainLyrics"]="primary plain first\nprimary plain second";
        s.reply=[&](const QUrl &u){return u.path().endsWith("/get")?qMakePair(404,QByteArray("{}")):qMakePair(200,QJsonDocument(QJsonArray{synced,plain}).toJson());};b.reply=[](const QUrl &){return qMakePair(200,QByteArray("{\"lyrics\":\"fallback &amp; first<br>fallback second\"}"));};
        LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint(),b.endpoint("/v1"));dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);tasks->setCurrentRow(0);auto *versions=dialog.findChild<QListWidget *>("lyricsVersions");QCOMPARE(versions->count(),3);auto *rolling=dialog.findChild<QCheckBox *>("lyricsSyncedOnly");auto *source=dialog.findChild<QComboBox *>("lyricsSourceFilter");rolling->setChecked(true);QCOMPARE(versions->count(),1);source->setCurrentText("Lyrics.ovh");QCOMPARE(versions->count(),1);QVERIFY(versions->item(0)->text().contains("Lyrics.ovh"));source->setCurrentIndex(0);QCOMPARE(versions->count(),3);source->setCurrentText("Lyrics.ovh");dialog.findChild<QPushButton *>("lyricsApply")->click();QFile f(l.absolute(l.track(t.id).lyrics));QVERIFY(f.open(QIODevice::ReadOnly));const auto content=f.readAll();QVERIFY(content.contains("fallback & first"));QVERIFY(!content.contains("primary"));dialog.close();
    }
    void v17OffsetPreservesEncodingAndCredibility(){
        const auto utf8=QByteArray("\xef\xbb\xbf[00:00.00]first\n[offset:100]\n[00:02.00]second");const auto shifted=LyricsLookup::withOffset(utf8,800);QVERIFY(shifted.startsWith("\xef\xbb\xbf"));QVERIFY(shifted.contains("[offset:800]"));QVERIFY(!shifted.contains("[offset:100]"));QCOMPARE(LyricsLookup::withOffset(utf8,0),utf8);QCOMPARE(LyricsLookup::withOffset("plain text",800),QByteArray("plain text"));
        QStringEncoder encoder(QStringConverter::Utf16LE);const QByteArray encoded=encoder(QString("[00:00.00]第一句\n[00:02.00]第二句"));const QByteArray utf16=QByteArray("\xff\xfe")+encoded;auto changed=LyricsLookup::withOffset(utf16,800);QVERIFY(changed.startsWith("\xff\xfe"));QCOMPARE(Lyrics::parse(changed).lines.first().time,qint64(800));QCOMPARE(Lyrics::parse(changed).lines.first().text,QString("第一句"));
        const auto gbk=QByteArray("[00:00.00]")+QByteArray::fromHex("b2e2cad4");QVERIFY(LyricsLookup::withOffset(gbk,800).endsWith(gbk));Track t;t.duration=10000;QString reason;QVERIFY(!LyricsLookup::credibleLyrics("[00:00.00]one",t,&reason));QVERIFY(reason.contains("两行"));QVERIFY(!LyricsLookup::credibleLyrics("[00:00.00]one\n[00:50.00]two",t));QVERIFY(!LyricsLookup::credibleLyrics("<div>one</div>\ntwo",t));QVERIFY(LyricsLookup::credibleLyrics("[00:00.00]one\n[00:02.00]two",t));
    }
    void v17ConfirmedInstrumentalIsPersistentAndNotSeekable(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"纯音乐验证","instrumental");V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));auto r=record(t.title);r["instrumental"]=true;r["syncedLyrics"]="";r["plainLyrics"]="";s.reply=[&](const QUrl &u){return qMakePair(200,QJsonDocument(u.path().endsWith("/get")?QJsonDocument(r):QJsonDocument(QJsonArray{r})).toJson());};LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(l.isInstrumental(t.id),5000);QFile f(l.absolute(l.track(t.id).lyrics));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(QString::fromUtf8(f.readAll()),QString("纯音乐，请欣赏"));dialog.close();
        const auto uncertain=seed(l,"待确认纯音乐","uncertain");auto unsure=r;unsure["trackName"]=uncertain.title;unsure["artistName"]="不同歌手";s.reply=[&](const QUrl &u){return qMakePair(200,QJsonDocument(u.path().endsWith("/get")?QJsonDocument(unsure):QJsonDocument(QJsonArray{unsure})).toJson());};LyricsLookup choice(&l,{uncertain.id},nullptr,s.endpoint());choice.show();relaxerClickSearch(&choice);auto *choices=choice.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(choices->item(0)->data(Qt::UserRole+2).toString().contains("需确认纯音乐"),5000);QVERIFY(!l.isInstrumental(uncertain.id));choices->setCurrentRow(0);choice.findChild<QPushButton *>("lyricsConfirmInstrumental")->click();QVERIFY(l.isInstrumental(uncertain.id));choice.close();
        LyricView view;view.resize(420,300);view.setLyrics(Lyrics::parse(QString("纯音乐，请欣赏").toUtf8()));view.show();QSignalSpy spy(&view,&LyricView::seekRequested);QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,view.rect().center());wheel(&view,-120);QCOMPARE(spy.count(),0);QVERIFY(!view.hasTimeline());
        l.setSetting("queue",QJsonArray{double(t.id)});l.setSetting("current",double(t.id));Window w(&l);w.show();w.showNowPlaying();QTest::qWait(500);QVERIFY(w.findChild<QDoubleSpinBox *>("lyricDelay")->parentWidget()->isHidden());capture(w,"v17-instrumental.png");
    }
    void v17LongLyricsScrollResumeAndGroupedTimestamps(){
        LyricView view;view.resize(320,300);const auto lyric=QString("[00:00.00]%1\n[00:00.00]translation\n[00:05.00]second sentence").arg(QString(90,QChar(0x6b4c)));view.setLyrics(Lyrics::parse(lyric.toUtf8()));view.setOffset(800);view.setPosition(0);view.show();QTest::qWait(50);QCOMPARE(view.property("lyricBlockCount").toInt(),2);QSignalSpy spy(&view,&LyricView::seekRequested);QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,view.rect().center());QCOMPARE(spy.count(),1);QCOMPARE(spy.at(0).at(0).toLongLong(),qint64(800));wheel(&view,-120);QVERIFY(view.property("manualLyric").toInt()>=0);QTRY_COMPARE_WITH_TIMEOUT(view.property("manualLyric").toInt(),-1,3500);capture(view,"v17-long-lyrics.png");
    }
    void v17ShuffleForecastMatchesActualPlaybackAcrossRounds(){
        Queue q;QVector<qint64> ids;for(int i=1;i<=12;++i)ids<<i;q.set(ids,1);q.setMode(Mode::Shuffle);
        for(int round=0;round<8;++round){const auto preview=q.upcoming();QCOMPARE(q.upcoming(),preview);for(auto id:preview)QCOMPARE(q.next(true),id);}
        q.setMode(Mode::SingleLoop);const auto current=q.current();const auto preview=q.upcoming();QCOMPARE(q.next(false),current);QCOMPARE(q.next(true),preview.first());q.set(ids,12);q.setMode(Mode::Sequential);QVERIFY(q.upcoming().isEmpty());
    }
    void v17QueueHeightWidthAndBackdropCache(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());QVector<Track> songs;for(int i=0;i<12;++i)songs<<seed(l,QString("超过六个汉字的歌曲名称%1").arg(i),QString("panel%1").arg(i),"超过四个汉字的歌手名称");QWidget root;root.resize(760,600);root.show();NextQueuePanel panel(&l,true,&root);panel.setTracks(songs);panel.setAnchor(QPoint(20,560),320);panel.present(true,false);QCOMPARE(panel.height(),94);panel.setExpanded(true,false);QCOMPARE(panel.height(),268);QVERIFY(panel.expanded());panel.invalidateBackground();const int builds=panel.property("backgroundBuilds").toInt();panel.grab();panel.grab();QCOMPARE(panel.property("backgroundBuilds").toInt(),builds);QFont font;font.setPixelSize(13);const QFontMetrics metrics(font);const auto title=NextQueuePanel::limitedText(songs[0].title,font,6);QVERIFY(metrics.horizontalAdvance(title)<=metrics.horizontalAdvance(QString(6,QChar(0x6b4c))));QVERIFY(title.contains(QChar(0x2026)));capture(panel,"v17-queue-expanded.png");
        QJsonArray queue;for(const auto &t:songs)queue<<double(t.id);l.setSetting("queue",queue);l.setSetting("current",double(songs[0].id));Window w(&l);w.resize(1240,790);w.show();QTest::qWait(100);w.showNowPlaying();QTest::qWait(500);QVERIFY(!w.findChild<QWidget *>("sidebar")->isHidden());QVERIFY(w.findChild<NextQueuePanel *>("nextQueueLarge")->isVisible());QVERIFY(w.findChild<NextQueuePanel *>("nextQueueCompact")->isHidden());capture(w,"v17-large-player.png");w.findChild<NextQueuePanel *>("nextQueueLarge")->setExpanded(true,false);QTest::qWait(50);capture(w,"v17-large-expanded.png");w.showLibrary();QTest::qWait(500);QVERIFY(!w.findChild<QWidget *>("sidebar")->isHidden());QVERIFY(w.findChild<NextQueuePanel *>("nextQueueCompact")->isVisible());capture(w,"v17-compact-player.png");
    }
    void v17TransientServerFailureRetriesOnce(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"重试验证","retry");V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));QHash<QString,int> calls;s.reply=[&](const QUrl &u){const auto key=u.toString();const int count=++calls[key];if(count==1)return qMakePair(503,QByteArray("{}"));return u.path().endsWith("/get")?qMakePair(404,QByteArray("{}")):qMakePair(200,QByteArray("[]"));};LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().contains("数据库无结果"),5000);for(auto count:calls)QCOMPARE(count,2);QVERIFY(l.lookupCache(t.id).fresh);dialog.close();
    }
    void v17AlbumAndCompactCjkQueriesAndCancellation(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"测 试 歌曲","queries");V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));s.reply=[](const QUrl &u){return u.path().endsWith("/get")?qMakePair(404,QByteArray("{}")):qMakePair(200,QByteArray("[]"));};LyricsLookup dialog(&l,{t.id},nullptr,s.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().contains("数据库无结果"),5000);bool album=false,compact=false;for(const auto &u:s.urls){const QUrlQuery q(u);if(u.path().endsWith("/search")){QVERIFY(!q.hasQueryItem("duration"));album|=q.hasQueryItem("album_name")&&!q.hasQueryItem("artist_name");compact|=q.queryItemValue("q")==QString("测试歌曲");}}QVERIFY(album);QVERIFY(compact);tasks->setCurrentRow(0);dialog.findChild<QLineEdit *>("lyricsSearchFragment")->setText("记得的歌词片段");dialog.findChild<QPushButton *>("lyricsManualSearch")->click();QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),5000);bool fragment=false;for(const auto &u:s.urls)fragment|=QUrlQuery(u).queryItemValue("q")==QString("记得的歌词片段");QVERIFY(fragment);dialog.close();
        QVector<qint64> ids;for(int i=0;i<5;++i)ids<<seed(l,QString("并发验证%1").arg(i),QString("parallel%1").arg(i)).id;s.hold=true;const int before=s.requests;LyricsLookup cancel(&l,ids,nullptr,s.endpoint());cancel.show();relaxerClickSearch(&cancel);QTRY_VERIFY(s.requests>=before+6);QVERIFY(cancel.property("parallelTracks").toInt()>=3);QVERIFY(cancel.property("parallelQueries").toInt()>=3);cancel.findChild<QPushButton *>("lyricsStop")->click();for(auto socket:s.held)if(socket)V16Server::send(socket,200,QJsonDocument(record("并发验证0")).toJson());QTest::qWait(200);for(auto id:ids){QVERIFY(l.track(id).lyrics.isEmpty());QVERIFY(!l.lookupCache(id).fresh);}cancel.close();
    }

    void v18OverlayKeepsUnderlyingPageAndFooterStable(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());QJsonArray queue;for(int i=0;i<4;++i)queue<<double(seed(l,QString("覆盖验证%1").arg(i),QString("overlay%1").arg(i)).id);l.setSetting("queue",queue);l.setSetting("current",queue.first());
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();w.showProfile();QTest::qWait(300);
        auto *pages=w.findChild<QStackedWidget *>("pages");auto *side=w.findChild<QWidget *>("sidebar");auto *content=w.findChild<QWidget *>("contentHost");auto *center=w.findChild<QWidget *>("playerCenter");const auto sideRect=side->geometry(),centerRect=center->geometry();QCOMPARE(pages->currentIndex(),2);
        w.showNowPlaying();QTest::qWait(70);QVERIFY(w.findChild<NextQueuePanel *>("nextQueueLarge")->property("queueAnimating").toBool());QTest::qWait(450);
        QCOMPARE(pages->currentIndex(),2);QVERIFY(side->isVisible());QCOMPARE(side->geometry(),sideRect);QCOMPARE(center->geometry(),centerRect);QCOMPARE(w.findChild<Backdrop *>()->geometry(),content->rect());
        auto *large=w.findChild<NextQueuePanel *>("nextQueueLarge");auto *bottom=w.findChild<QStackedWidget *>("bottomStack");QVERIFY(large->y()>=bottom->y());QVERIFY(large->height()<=bottom->height());QCOMPARE(large->x(),8);
        auto *volume=w.findChild<QWidget *>("volumeControls");QCOMPARE(volume->y(),(volume->parentWidget()->height()-volume->height())/2);QTest::mouseMove(large,QPoint(45,12));QCOMPARE(large->property("hoveredQueueRow").toInt(),-2);capture(w,"v18-large-footer.png");
        QTest::keyClick(&w,Qt::Key_Escape);QTest::qWait(500);QCOMPARE(pages->currentIndex(),2);QVERIFY(!w.findChild<Backdrop *>()->isVisible());QVERIFY(w.findChild<NextQueuePanel *>("nextQueueCompact")->isVisible());QCOMPARE(side->geometry(),sideRect);QCOMPARE(center->geometry(),centerRect);
    }
    void v18BatchActionsSaveOnlyCheckedTracks(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"批量甲","batch-a"),b=seed(l,"批量乙","batch-b"),c=seed(l,"批量丙","batch-c");const QByteArray lyric("[00:00.00]first line\n[00:01.00]second line");for(auto id:{a.id,b.id,c.id})QVERIFY(l.saveLyrics(id,lyric));V16Server s;QVERIFY(s.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&l,{a.id,b.id,c.id},nullptr,s.endpoint());dialog.show();dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);QTRY_VERIFY(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled());auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");tracks->setCurrentRow(0);dialog.findChild<QPushButton *>("lyricsMultiSelect")->click();tracks->item(1)->setSelected(true);
        QVERIFY(dialog.findChild<QWidget *>("lyricsMorePanel")->isVisible());QVERIFY(!dialog.findChild<QPushButton *>("lyricsMore")->isEnabled());QVERIFY(!dialog.findChild<QPushButton *>("lyricsManualSearch")->isEnabled());QVERIFY(!dialog.findChild<QPushButton *>("lyricsPaste")->isEnabled());QVERIFY(!dialog.findChild<QPushButton *>("lyricsImport")->isEnabled());QVERIFY(dialog.findChild<QPushButton *>("lyricsSaveOffset")->isEnabled());
        dialog.findChild<QSpinBox *>("lyricsOffset")->setValue(800);dialog.findChild<QPushButton *>("lyricsSaveOffset")->click();for(auto id:{a.id,b.id}){QFile f(l.absolute(l.track(id).lyrics));QVERIFY(f.open(QIODevice::ReadOnly));QVERIFY(f.readAll().contains("[offset:800]"));}QFile unchanged(l.absolute(l.track(c.id).lyrics));QVERIFY(unchanged.open(QIODevice::ReadOnly));QCOMPARE(unchanged.readAll(),lyric);unchanged.close();
        QTimer::singleShot(0,&dialog,[]{for(auto *widget:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox *>(widget))box->button(QMessageBox::Yes)->click();});dialog.findChild<QPushButton *>("lyricsMarkInstrumental")->click();QVERIFY(l.isInstrumental(a.id));QVERIFY(l.isInstrumental(b.id));QVERIFY(!l.isInstrumental(c.id));QCOMPARE(s.requests,0);capture(dialog,"v18-lyrics-multi.png");dialog.close();
    }
    void v18ToolsIdleAndPinnedDelayPanel(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"歌词工具","tools");QVERIFY(l.saveLyrics(t.id,QByteArray("[00:00.00]first\n[00:01.00]second\n[00:02.00]third")));l.setSetting("queue",QJsonArray{double(t.id)});l.setSetting("current",double(t.id));Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();w.showNowPlaying();QTest::qWait(500);
        auto *view=w.findChild<LyricView *>();auto *question=w.findChild<QPushButton *>("lyricToolsQuestion");auto *scroll=w.findChild<QScrollBar *>("lyricScrollBar");QVERIFY(!question->isVisible());QTest::mouseMove(view,view->rect().center());QTRY_VERIFY(question->isVisible());QVERIFY(scroll->isVisible());question->click();QTRY_VERIFY(w.findChild<QFrame *>("lyricToolsMenu")->isVisible());QTRY_VERIFY_WITH_TIMEOUT(!question->isVisible(),3500);QVERIFY(!w.findChild<QFrame *>("lyricToolsMenu")->isVisible());
        QTest::mouseMove(view,view->rect().center()+QPoint(2,0));QTRY_VERIFY(question->isVisible());question->click();w.findChild<QPushButton *>("lyricDelayEntry")->click();QTest::qWait(3200);QVERIFY(question->isVisible());QVERIFY(w.findChild<QDoubleSpinBox *>("lyricDelay")->isVisible());capture(w,"v18-lyric-tools.png");QTest::mouseClick(w.findChild<Backdrop *>(),Qt::LeftButton,Qt::NoModifier,QPoint(25,130));QTRY_VERIFY(!question->isVisible());
    }
    void v18SettingsScrollHasIntermediateFrames(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());seed(l,"列宽滚动","scroll-column");Window w(&l);w.show();w.showSettings();QTest::qWait(300);auto *area=w.findChild<QScrollArea *>("settingsScroll");auto *bar=area->verticalScrollBar();QVERIFY(bar->maximum()>0);wheel(area->viewport(),-120);const int target=bar->property("smoothScrollTarget").toInt();QVERIFY(target>0);QVERIFY(bar->value()<target);QTest::qWait(55);QVERIFY(bar->value()>0);QVERIFY(bar->value()<target);QTRY_COMPARE(bar->value(),target);
        capture(w,"v18-settings.png");w.showLibrary();auto *table=w.findChild<SongTable *>("songTable");QTRY_VERIFY(table->isVisible());table->horizontalHeader()->resizeSection(SongTable::ArtistColumn,210);QTRY_COMPARE(table->horizontalHeader()->length(),table->viewport()->width());
    }


    void v19QueueFitsItsTextAndCollapseReturns(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"旧歌曲标题","v19-a"),b=seed(l,"新的歌曲标题","v19-b"),c=seed(l,"第三首","v19-c");
        l.setSetting("queue",QJsonArray{double(a.id),double(b.id),double(c.id)});l.setSetting("current",double(a.id));
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();QTest::qWait(300);
        auto *compact=w.findChild<NextQueuePanel *>("nextQueueCompact");QVERIFY(compact->width()<360);compact->setExpanded(true,false);QVERIFY(compact->width()<320);QCOMPARE(compact->geometry().right(),w.centralWidget()->width()-9);capture(w,"v19-queue-normal.png");
        w.showNowPlaying();QTest::qWait(500);auto *large=w.findChild<NextQueuePanel *>("nextQueueLarge");QVERIFY(large->width()<320);QCOMPARE(large->x(),8);large->setExpanded(true,false);QVERIFY(large->width()<320);QCOMPARE(large->x(),8);capture(w,"v19-queue-large.png");
        auto *flight=w.findChild<ArtworkFlight *>();QTest::mouseClick(large,Qt::LeftButton,Qt::NoModifier,QPoint(50,12));QVERIFY(flight->isVisible());QCOMPARE(w.playback()->current().id,b.id);QVERIFY(!w.findChild<QLabel *>("nowTitle")->isHidden());QTRY_VERIFY(!flight->isVisible());
        IconButton *next=nullptr,*previous=nullptr,*collapse=nullptr;for(auto *button:w.findChildren<IconButton *>()){const auto name=button->property("symbolName").toString();if(name=="next")next=button;else if(name=="previous")previous=button;else if(name=="collapse")collapse=button;}
        QVERIFY(next);QVERIFY(previous);QVERIFY(collapse);QCOMPARE(collapse->toolTip(),QString("收起"));
        next->click();QVERIFY(flight->isVisible());QCOMPARE(w.playback()->current().id,c.id);QTest::qWait(50);previous->click();QVERIFY(flight->isVisible());QCOMPARE(w.playback()->current().id,b.id);QTRY_VERIFY(!flight->isVisible());QVERIFY(!w.findChild<Cover *>("nowCover")->isHidden());
        collapse->click();QTest::qWait(500);QVERIFY(!w.findChild<Backdrop *>()->isVisible());QVERIFY(compact->isVisible());QCOMPARE(compact->geometry().right(),w.centralWidget()->width()-9);
    }
    void v19CoverPocketClipsBothDirections(){
        QWidget root;root.resize(420,360);auto palette=root.palette();palette.setColor(QPalette::Window,Qt::blue);root.setPalette(palette);root.setAutoFillBackground(true);root.show();
        MovingArtwork art;art.coverOld=art.coverTo=QRectF(100,60,160,160);art.coverOutgoing=QPixmap(160,160);art.coverOutgoing.fill(Qt::red);art.cover=QPixmap(160,160);art.cover.fill(Qt::green);
        ArtworkFlight flight(&root);bool complete=false;flight.start(art,[&]{complete=true;},1);QTest::qWait(80);QVERIFY(flight.isVisible());auto forward=root.grab().toImage();
        QCOMPARE(forward.pixelColor(110,140),QColor(Qt::red));QCOMPARE(forward.pixelColor(250,140),QColor(Qt::green));QCOMPARE(forward.pixelColor(280,140),QColor(Qt::blue));capture(root,"v19-cover-next.png");QTRY_VERIFY(complete);
        complete=false;flight.start(art,[&]{complete=true;},-1);QTest::qWait(80);auto backward=root.grab().toImage();QCOMPARE(backward.pixelColor(110,140),QColor(Qt::green));QCOMPARE(backward.pixelColor(250,140),QColor(Qt::red));QCOMPARE(backward.pixelColor(280,140),QColor(Qt::blue));capture(root,"v19-cover-previous.png");QTRY_VERIFY(complete);
    }
    void v19QueueKeepsOldCoverOpaqueWithoutTextAnimation(){
        QWidget root;root.resize(420,400);auto palette=root.palette();palette.setColor(QPalette::Window,Qt::blue);root.setPalette(palette);root.setAutoFillBackground(true);root.show();
        MovingArtwork art;art.coverOld=art.coverTo=QRectF(100,60,160,160);art.coverFrom=QRectF(20,330,24,24);art.coverOutgoing=QPixmap(160,160);art.coverOutgoing.fill(Qt::red);art.cover=QPixmap(160,160);art.cover.fill(Qt::green);
        art.titleOld=QRectF(100,250,40,12);art.artistOld=QRectF(100,270,40,12);art.titleOutgoing=art.artistOutgoing=QPixmap(40,12);art.titleOutgoing.fill(Qt::red);art.artistOutgoing.fill(Qt::red);
        art.titleFrom=QRectF(50,330,40,12);art.artistFrom=QRectF(50,350,40,12);art.titleTo=art.titleOld;art.artistTo=art.artistOld;art.titleSmall=art.titleLarge=art.artistSmall=art.artistLarge=QPixmap(40,12);art.titleSmall.fill(Qt::yellow);art.titleLarge.fill(Qt::yellow);art.artistSmall.fill(Qt::yellow);art.artistLarge.fill(Qt::yellow);
        ArtworkFlight flight(&root);bool complete=false;flight.start(art,[&]{complete=true;});QTest::qWait(30);QVERIFY(flight.isVisible());const auto frame=root.grab().toImage();QCOMPARE(frame.pixelColor(180,140),QColor(Qt::red));QCOMPARE(frame.pixelColor(110,255),QColor(Qt::blue));QCOMPARE(frame.pixelColor(110,275),QColor(Qt::blue));
        for(int y=0;y<frame.height();++y)for(int x=0;x<frame.width();++x){const auto color=frame.pixelColor(x,y);if(color.red()>0&&color.green()==0)QVERIFY(art.coverOld.contains(QPointF(x,y)));}
        capture(root,"v19-queue-flight.png");QTRY_VERIFY(complete);
    }



    void v19SecondToastDoesNotAddBottomStrip(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());Window w(&l);w.show();QTest::qWait(200);const auto rect=w.centralWidget()->geometry();
        QVERIFY(QMetaObject::invokeMethod(w.playback(),"error",Qt::DirectConnection,Q_ARG(QString,QString("第一条提示"))));
        auto *notice=w.findChild<QLabel *>("toastNotice");QVERIFY(notice);QVERIFY(notice->isVisible());QCOMPARE(w.centralWidget()->geometry(),rect);QVERIFY(w.statusBar()->isHidden());
        auto *timer=w.findChild<QTimer *>("toastDismissTimer");QVERIFY(timer);timer->setInterval(60);
        QVERIFY(QMetaObject::invokeMethod(w.playback(),"error",Qt::DirectConnection,Q_ARG(QString,QString("第二条提示"))));QCOMPARE(notice->text(),QString("第二条提示"));
        QTRY_VERIFY_WITH_TIMEOUT(notice->isHidden(),500);QCOMPARE(w.centralWidget()->geometry(),rect);QVERIFY(w.statusBar()->isHidden());
    }
    void v19SecondQueueCoversAndCenteredNames(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"封面位置保持原样","r2-align-a"),b=seed(l,"超过八个汉字的歌曲名称","r2-align-b","超过五个汉字的歌手"),c=seed(l,"第三首","r2-align-c");l.setSetting("queue",QJsonArray{double(a.id),double(b.id),double(c.id)});l.setSetting("current",double(a.id));
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();w.showNowPlaying();QTest::qWait(500);
        auto *cover=w.findChild<Cover *>("nowCover");auto *title=w.findChild<QLabel *>("nowTitle");auto *artist=w.findChild<QLabel *>("nowArtist");auto *root=w.centralWidget();const auto coverRect=artworkRect(cover,root);const int titleY=title->mapTo(root,QPoint()).y(),artistY=artist->mapTo(root,QPoint()).y();
        title->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);artist->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);QCoreApplication::processEvents();QCOMPARE(artworkRect(cover,root),coverRect);
        title->setAlignment(Qt::AlignCenter);artist->setAlignment(Qt::AlignCenter);QCoreApplication::processEvents();QCOMPARE(artworkRect(cover,root),coverRect);QCOMPARE(title->mapTo(root,QPoint()).y(),titleY);QCOMPARE(artist->mapTo(root,QPoint()).y(),artistY);
        QVERIFY(qAbs(title->mapTo(root,title->rect().center()).x()-coverRect.center().x())<=1);QVERIFY(qAbs(artist->mapTo(root,artist->rect().center()).x()-coverRect.center().x())<=1);
        auto *large=w.findChild<NextQueuePanel *>("nextQueueLarge");auto *bottom=w.findChild<QStackedWidget *>("bottomStack");QCOMPARE(large->height(),bottom->height()-4);QCOMPARE(large->y(),bottom->mapTo(root,QPoint()).y());
        const auto first=large->artwork(0);QFont font=large->font();font.setPixelSize(13);font.setWeight(QFont::Normal);QFontMetrics metrics(font);QCOMPARE(first.titleFrom.width(),qreal(metrics.horizontalAdvance(QString(8,QChar(0x6b4c)))));QCOMPARE(first.artistFrom.width(),qreal(metrics.horizontalAdvance(QString(5,QChar(0x6b4c)))));QCOMPARE(first.coverFrom.width(),qreal(40));QCOMPARE(first.titleFrom.height(),qreal(large->height()-40));capture(w,"v19-r2-centered-player.png");
        w.showLibrary();QTest::qWait(500);auto *compact=w.findChild<NextQueuePanel *>("nextQueueCompact");const auto art=compact->artwork(0);QCOMPARE(art.coverFrom.width(),qreal(24));const auto local=art.coverFrom.toRect().translated(-compact->pos());const auto image=compact->grab().toImage();const auto actual=image.pixelColor(local.center());const auto wanted=coverPixmap(l.absolute(b.cover),b.title,80).toImage().pixelColor(40,40);QVERIFY(qAbs(actual.red()-wanted.red())<8);QVERIFY(qAbs(actual.green()-wanted.green())<8);QVERIFY(qAbs(actual.blue()-wanted.blue())<8);capture(w,"v19-r2-right-collapsed.png");
        compact->setExpanded(true,false);QCOMPARE(compact->artwork(0).coverFrom.width(),qreal(26));QCOMPARE(compact->geometry().right(),root->width()-9);capture(w,"v19-r2-right-expanded.png");
    }
    void v19SecondTrackAnimationCancelsQueuedPageMotion(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"旧歌曲文字","r2-motion-a"),b=seed(l,"新歌曲文字","r2-motion-b"),c=seed(l,"下一首验证","r2-motion-c");l.setSetting("queue",QJsonArray{double(a.id),double(b.id),double(c.id)});l.setSetting("current",double(a.id));
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(250);auto *page=w.findChild<QWidget *>("pageMotion");auto *flight=w.findChild<ArtworkFlight *>();auto *large=w.findChild<NextQueuePanel *>("nextQueueLarge");
        w.showNowPlaying();QVERIFY(QMetaObject::invokeMethod(large,"activated",Qt::DirectConnection,Q_ARG(int,0)));QCoreApplication::processEvents();QTest::qWait(30);QVERIFY(!page->isVisible());QVERIFY(flight->isVisible());QVERIFY(!w.findChild<QLabel *>("nowTitle")->isHidden());QCOMPARE(w.playback()->current().id,b.id);
        const auto rect=w.centralWidget()->geometry();QVERIFY(QMetaObject::invokeMethod(w.playback(),"error",Qt::DirectConnection,Q_ARG(QString,QString("动画期间提示"))));QCOMPARE(w.centralWidget()->geometry(),rect);QVERIFY(flight->isVisible());QTRY_VERIFY(!flight->isVisible());
        w.showLibrary();w.showNowPlaying();IconButton *next=nullptr,*previous=nullptr;for(auto *button:w.findChildren<IconButton *>()){const auto name=button->property("symbolName").toString();if(name=="next")next=button;else if(name=="previous")previous=button;}QVERIFY(next);QVERIFY(previous);next->click();QCoreApplication::processEvents();QTest::qWait(30);QVERIFY(!page->isVisible());QVERIFY(flight->isVisible());QCOMPARE(w.playback()->current().id,c.id);
        previous->click();QCoreApplication::processEvents();QVERIFY(!page->isVisible());QCOMPARE(w.playback()->current().id,b.id);QTRY_VERIFY(!flight->isVisible());QVERIFY(!w.findChild<QLabel *>("nowTitle")->isHidden());QVERIFY(!w.findChild<QLabel *>("nowArtist")->isHidden());
    }


    void liveSongComparison(){
        if(qEnvironmentVariableIsEmpty("RELAXER_LIVE_QA"))QSKIP("实网对比单独按需运行");
        struct Sample{QString title,artist,album,label;int seconds;};
        const QList<Sample> samples={
            {"HandClap","Fitz and The Tantrums","Fitz and The Tantrums","截图歌曲",193},
            {"Remember Our Summer","FrogMonster 蛙蛙","Remember Our Summer","截图歌曲，双语歌手标签",163},
            {"We Are Electric","Flying Steps","B-Town","截图歌曲",208},
            {"Auf Und Auf Voll Lebenslust","Franzl Lang","Aus Freude Am Leben","截图歌曲，专辑按可见前缀补全",153},
            {"Love Ain't Changin","GhostDragon","","截图歌曲，歌手按可见部分",197},
            {"花 feat. 花譜","Guiano/花譜","花鳥風月","截图歌曲",208},
            {"01. HandClap [FLAC]","Fitz and The Tantrums","Fitz and The Tantrums","同曲模拟文件序号、音质标识",193},
            {"02. Remember Our Summer [320kbps]","FrogMonster 蛙蛙","Remember Our Summer","同曲模拟文件序号、音质标识",163}
        };
        QNetworkProxyFactory::setUseSystemConfiguration(true);QNetworkAccessManager network;
        auto legacyConfident=[](const Track &t,const QJsonObject &r){return !t.title.isEmpty()&&!t.artist.isEmpty()&&t.artist!="未知歌手"&&t.duration>0&&
            LyricsLookup::normalized(t.title)==LyricsLookup::normalized(r["trackName"].toString())&&LyricsLookup::normalized(t.artist)==LyricsLookup::normalized(r["artistName"].toString())&&
            qAbs(t.duration/1000.-r["duration"].toDouble(-1))<=2.&&
            (t.album.isEmpty()||t.album=="未知专辑"||LyricsLookup::normalized(t.album)==LyricsLookup::normalized(r["albumName"].toString()));};
        auto get=[&](const Track &t,int stage,int &calls){
            QUrl u("https://lrclib.net/api"+QString(stage==0?"/get":"/search"));QUrlQuery q;q.addQueryItem("track_name",t.title);
            if(stage<2)q.addQueryItem("artist_name",t.artist);
            if(stage==0){q.addQueryItem("album_name",t.album);q.addQueryItem("duration",QString::number(t.duration/1000.,'f',3));}u.setQuery(q);
            QNetworkRequest req(u);req.setHeader(QNetworkRequest::UserAgentHeader,"Relaxer/1.5");req.setTransferTimeout(12000);
            auto *reply=network.get(req);++calls;QEventLoop loop;connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QTimer::singleShot(13000,&loop,&QEventLoop::quit);loop.exec();
            const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto data=reply->readAll();if(!reply->isFinished())reply->abort();reply->deleteLater();
            return qMakePair(status,QJsonDocument::fromJson(data));
        };
        QJsonArray rows;int index=0;QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());
        for(const auto &s:samples){
            auto t=seed(l,s.title,QString("live%1").arg(++index),s.artist);t.album=s.album;t.duration=s.seconds*1000;
            // Seed() writes a short synthetic media file; the comparison metadata comes from the screenshot.
            const auto connection=QSqlDatabase::connectionNames();for(const auto &name:connection){auto db=QSqlDatabase::database(name);if(db.databaseName().contains(dir.path())){QSqlQuery update(db);update.prepare("UPDATE tracks SET album=?,duration=? WHERE id=?");update.addBindValue(t.album);update.addBindValue(t.duration);update.addBindValue(t.id);QVERIFY(update.exec());break;}}
            int oldCalls=0;QElapsedTimer timer;timer.start();QJsonArray records;bool oldAutomatic=false;
            auto first=get(t,0,oldCalls);
            if(first.first==200&&legacyConfident(t,first.second.object())&&(!first.second.object()["syncedLyrics"].toString().isEmpty()||!first.second.object()["plainLyrics"].toString().isEmpty()))records.append(first.second.object());
            else if(first.first==200||first.first==400||first.first==404){
                auto search=get(t,1,oldCalls);records=search.second.array();
                if(search.first==200&&records.isEmpty())records=get(t,2,oldCalls).second.array();
            }
            int oldCandidates=0;for(const auto &v:records){auto r=v.toObject();if(!r["syncedLyrics"].toString().isEmpty()||!r["plainLyrics"].toString().isEmpty()){++oldCandidates;if(legacyConfident(t,r))oldAutomatic=true;}}
            const qint64 oldMs=timer.elapsed();int newCalls=0;
            LyricsLookup dialog(&l,{t.id});dialog.findChild<QCheckBox *>("lyricsReplace")->setChecked(true);auto *manager=dialog.findChild<QNetworkAccessManager *>();QVERIFY(manager);connect(manager,&QNetworkAccessManager::finished,&dialog,[&](QNetworkReply *){++newCalls;});
            timer.restart();dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");
            while(timer.elapsed()<38000){QTest::qWait(20);auto status=tasks->item(0)->data(Qt::UserRole+2).toString();if(!status.startsWith("等待")&&!status.contains("正在")&&dialog.findChild<QPushButton *>("lyricsStart")->isEnabled())break;}
            const qint64 newMs=timer.elapsed();tasks->setCurrentRow(0);const int newCandidates=dialog.findChild<QListWidget *>("lyricsVersions")->count();
            const QString status=tasks->item(0)->data(Qt::UserRole+2).toString();
            QJsonObject row{{"title",s.title},{"artist",s.artist},{"note",s.label},{"v15_first_http",first.first},{"v15_candidates",oldCandidates},{"v15_auto",oldAutomatic},{"v15_requests",oldCalls},{"v15_ms",oldMs},{"v16_candidates",newCandidates},{"v16_auto",!l.track(t.id).lyrics.isEmpty()},{"v16_requests",newCalls},{"v16_ms",newMs},{"v16_status",status}};
            rows.append(row);qInfo().noquote()<<QJsonDocument(row).toJson(QJsonDocument::Compact);dialog.close();
            const auto file=qEnvironmentVariable("RELAXER_BENCHMARK_FILE");if(!file.isEmpty()){QFile out(file);QVERIFY(out.open(QIODevice::WriteOnly));out.write(QJsonDocument(rows).toJson());}QTest::qWait(300);
        }
    }

};
QTEST_MAIN(V16Tests)
#include "v16-tests.moc"
