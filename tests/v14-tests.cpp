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
using namespace relaxer;
class FixtureServer:public QTcpServer {
public:
    int requests=0;bool hold=false;QList<QUrl> urls;
    std::function<QPair<int,QByteArray>(const QUrl &)> reply;
    FixtureServer(){connect(this,&QTcpServer::newConnection,this,[this]{auto *socket=nextPendingConnection();socket->setProperty("requestBuffer",QByteArray());connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);connect(socket,&QTcpSocket::readyRead,socket,[this,socket]{auto bytes=socket->property("requestBuffer").toByteArray()+socket->readAll();socket->setProperty("requestBuffer",bytes);if(!bytes.contains("\r\n\r\n")||socket->property("answered").toBool())return;socket->setProperty("answered",true);const auto target=bytes.split(' ').value(1);const QUrl url=QUrl::fromEncoded("http://localhost"+target);urls<<url;++requests;if(hold)return;const auto response=reply(url);const auto body=response.second;socket->write("HTTP/1.1 "+QByteArray::number(response.first)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});}
    QUrl endpoint()const{return QUrl(QString("http://127.0.0.1:%1/api").arg(serverPort()));}
};
class V14Tests:public QObject {
    Q_OBJECT
    static Track seed(Library &l,const QString &title="测试歌曲",const QString &hash="seed",const QString &artist="歌手甲",const QString &album="专辑一"){
        Track t;t.title=title;t.hash=hash;t.artist=artist;t.album=album;t.file="Music/"+hash+".wav";t.duration=10000;t.coverOrigin="none";
        QFile file(l.absolute(t.file));if(!file.open(QIODevice::WriteOnly))return {};QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(160000);out.writeRawData(QByteArray(160000,'\0').constData(),160000);file.close();t.id=l.insert(t);return l.track(t.id);
    }
    static void capture(QWidget &w,const QString &name){const auto dir=qEnvironmentVariable("RELAXER_QA_DIR");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.grab().save(dir+"/v14-"+name));}}
    static void enter(Window &w,SongTable *table){
        table->selectRow(0);const QPoint point=table->visualItemRect(table->item(0,SongTable::TitleColumn)).center();
        QTimer::singleShot(30,&w,[]{auto *menu=qobject_cast<QMenu *>(QApplication::activePopupWidget());if(!menu)return;for(auto *action:menu->actions())if(action->objectName()=="enterMultiSelect"){QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());return;}menu->close();});
        QMetaObject::invokeMethod(table,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,point));
    }
    static void playlist(Window &w,qint64 id){auto *nav=w.findChild<QListWidget *>("playlistNavigation");for(int i=0;i<nav->count();++i)if(nav->item(i)->data(Qt::UserRole).toLongLong()==id){QTest::mouseClick(nav->viewport(),Qt::LeftButton,Qt::NoModifier,nav->visualItemRect(nav->item(i)).center());return;}}
    static void choose(FilterButton *button,const QString &text){button->click();auto *popup=QApplication::activePopupWidget();QVERIFY(popup);auto *search=popup->findChild<QLineEdit *>("filterSearch");auto *list=popup->findChild<QListWidget *>("filterOptions");QVERIFY(search);search->setText(text);QTest::qWait(10);for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==text){QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(i)).center());return;}popup->close();QFAIL("Filter candidate missing");}
    static QJsonObject lyricRecord(const Track &t){return {{"trackName",t.title},{"artistName",t.artist},{"albumName",t.album},{"duration",10.},{"syncedLyrics",QString("[00:00.00]原创测试第一行\n[00:02.00]原创测试第二行")},{"plainLyrics",QString("原创测试第一行\n原创测试第二行")}};}
private slots:
    void initTestCase(){qApp->setStyle("Fusion");}
    void managedDeletionAndRollback(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"甲","a"),b=seed(l,"乙","b");const auto p=l.createPlaylist("测试");QVERIFY(l.addToPlaylist(p,{a.id,b.id}));QVERIFY(l.favorites({a.id,b.id},true));const auto original=dir.filePath("original.wav");QVERIFY(QFile::copy(l.absolute(a.file),original));
        QVERIFY(l.removeManaged({a.id}));QVERIFY(!l.track(a.id).id);QVERIFY(!QFileInfo::exists(l.absolute(a.file)));QVERIFY(QFileInfo::exists(original));QCOMPARE(l.tracks(p).size(),1);
        auto db=QSqlDatabase::database();Q_UNUSED(db);const auto connection=QSqlDatabase::connectionNames();QSqlDatabase actual;for(const auto &name:connection){auto candidate=QSqlDatabase::database(name);if(candidate.databaseName().contains(dir.path())){actual=candidate;break;}}QVERIFY(actual.isValid());QSqlQuery query(actual);QVERIFY(query.exec("CREATE TRIGGER reject_delete BEFORE DELETE ON tracks BEGIN SELECT RAISE(ABORT,'fixture rollback'); END"));QVERIFY(!l.removeManaged({b.id}));QVERIFY(l.track(b.id).id);QVERIFY(QFileInfo::exists(l.absolute(b.file)));QVERIFY(query.exec("DROP TRIGGER reject_delete"));
        Track external=b;external.hash="external";external.file=original;external.id=l.insert(external);QVERIFY(external.id);QVERIFY(!l.removeManaged({external.id}));QVERIFY(QFileInfo::exists(original));QVERIFY(l.track(external.id).id);
    }
    void frozenSettingsAndProfileCollapse(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());for(int i=0;i<24;++i)seed(l,QString("歌曲 %1").arg(i),QString::number(i));Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();QTest::qWait(60);w.showSettings();QTest::qWait(260);auto *header=w.findChild<QWidget *>("settingsHeader");auto *scroll=w.findChild<QScrollArea *>("settingsScroll");QVERIFY(header);const QPoint anchor=header->mapTo(&w,QPoint());scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QCOMPARE(header->mapTo(&w,QPoint()),anchor);capture(w,"settings-frozen.png");scroll->verticalScrollBar()->setValue(0);QCOMPARE(header->mapTo(&w,QPoint()),anchor);
        w.showProfile();QTest::qWait(260);auto *profile=w.findChild<ProfileHeader *>();QCOMPARE(profile->height(),192);auto *recent=w.findChild<SongTable *>("recentTable");recent->setSongs(l.tracks(),l.root());QTRY_VERIFY(recent->verticalScrollBar()->maximum()>120);recent->verticalScrollBar()->setValue(120);QTest::qWait(230);QCOMPARE(profile->height(),80);capture(w,"profile-compact.png");recent->verticalScrollBar()->setValue(0);QTest::qWait(230);QCOMPARE(profile->height(),192);capture(w,"profile-expanded.png");
    }
    void searchableFiltersAndIntersection(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"甲","a","甲/乙","专辑一"),b=seed(l,"乙","b","乙","专辑二"),c=seed(l,"丙","c","丙","专辑三");Q_UNUSED(c);Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();QTest::qWait(60);auto *table=w.findChild<SongTable *>("songTable");auto *artist=w.findChild<FilterButton *>("artistFilter"),*album=w.findChild<FilterButton *>("albumFilter");choose(artist,"乙");QCOMPARE(table->rowCount(),2);QVERIFY(table->ids().contains(a.id));QVERIFY(table->ids().contains(b.id));choose(album,"专辑一");QCOMPARE(table->ids(),QVector<qint64>{a.id});capture(w,"filters.png");w.findChild<QPushButton *>("clearFilters")->click();QCOMPARE(table->rowCount(),3);
        artist->click();auto *popup=QApplication::activePopupWidget();popup->findChild<QLineEdit *>("filterSearch")->setText("不存在");QCOMPARE(popup->findChild<QListWidget *>("filterOptions")->count(),1);capture(*popup,"filter-search.png");popup->close();
    }
    void bulkActionsConfirmationAndExit(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"甲","a"),b=seed(l,"乙","b");const auto p=l.createPlaylist("歌单");QVERIFY(l.addToPlaylist(p,{a.id,b.id}));Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(80);auto *table=w.findChild<SongTable *>("songTable");enter(w,table);QVERIFY(table->multiSelect());QCOMPARE(w.findChild<QStackedWidget *>("bottomStack")->currentIndex(),1);w.findChild<QPushButton *>("bulkSelectAll")->click();QCOMPARE(table->selectedIds().size(),2);QVERIFY(w.findChild<QPushButton *>("bulkDelete")->isEnabled());QVERIFY(!w.findChild<QPushButton *>("bulkRemovePlaylist")->isEnabled());w.findChild<QPushButton *>("bulkLike")->click();QVERIFY(l.track(a.id).favorite&&l.track(b.id).favorite);QCOMPARE(table->selectedIds().size(),2);capture(w,"bulk-local.png");
        QTimer::singleShot(20,&w,[]{auto *dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->reject();});w.findChild<QPushButton *>("bulkDelete")->click();QVERIFY(table->multiSelect());QCOMPARE(table->selectedIds().size(),2);
        w.findChild<QPushButton *>("bulkExit")->click();playlist(w,p);QTest::qWait(260);enter(w,table);w.findChild<QPushButton *>("bulkSelectAll")->click();QVERIFY(!w.findChild<QPushButton *>("bulkDelete")->isEnabled());QVERIFY(w.findChild<QPushButton *>("bulkRemovePlaylist")->isEnabled());capture(w,"bulk-playlist.png");
        QTimer::singleShot(20,&w,[]{auto *dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QPushButton *>("removePlaylistOnly")->click();});w.findChild<QPushButton *>("bulkRemovePlaylist")->click();QVERIFY(!table->multiSelect());QVERIFY(l.tracks(p).isEmpty());QVERIFY(l.track(a.id).id);QVERIFY(QFileInfo::exists(l.absolute(a.file)));
        w.showLibrary();QTest::qWait(260);enter(w,table);w.findChild<QPushButton *>("bulkSelectAll")->click();QTimer::singleShot(20,&w,[]{auto *dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QPushButton *>("confirmManagedDelete")->click();});w.findChild<QPushButton *>("bulkDelete")->click();QVERIFY(!table->multiSelect());QVERIFY(l.tracks().isEmpty());QVERIFY(!QFileInfo::exists(l.absolute(a.file)));
    }
    void windowSizePersists(){
        QTemporaryDir dir;
        Library l(dir.filePath("data"));
        QVERIFY(l.open());
        {
            Window w(&l);
            if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))
                w.playback()->media()->setAudioOutput(nullptr);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            QCoreApplication::processEvents();
            w.resize(1110,690);
            // Saving is debounced by 350 ms and later resize/state events can
            // restart it. Wait for the stored result, not a 400 ms wall clock.
            QTRY_COMPARE_WITH_TIMEOUT(l.setting("windowWidth").toInt(),1110,3000);
            QTRY_COMPARE_WITH_TIMEOUT(l.setting("windowHeight").toInt(),690,3000);
            w.showMaximized();
            QTRY_VERIFY_WITH_TIMEOUT(w.isMaximized(),3000);
            QTRY_VERIFY_WITH_TIMEOUT(l.setting("windowMaximized").toBool(),3000);
            QCOMPARE(l.setting("windowWidth").toInt(),1110);
            QCOMPARE(l.setting("windowHeight").toInt(),690);
            w.showNormal();
            QTRY_VERIFY_WITH_TIMEOUT(!w.isMaximized(),3000);
            QTRY_VERIFY_WITH_TIMEOUT(!l.setting("windowMaximized").toBool(),3000);
        }
        {
            Window w(&l);
            if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))
                w.playback()->media()->setAudioOutput(nullptr);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            const auto available=QGuiApplication::primaryScreen()->availableGeometry().size();
            QTRY_COMPARE_WITH_TIMEOUT(w.size(),QSize(qMax(1000,qMin(1110,available.width())),qMax(660,qMin(690,available.height()))),3000);
        }
    }
    void windowSizePersistsAfterResizeBurst(){
        QTemporaryDir dir;
        Library l(dir.filePath("data"));
        QVERIFY(l.open());
        Window w(&l);
        if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))
            w.playback()->media()->setAudioOutput(nullptr);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QCoreApplication::processEvents();
        w.resize(1090,680);
        // A later resize legitimately moves the save past the old 400 ms
        // assertion. It must eventually persist the final, exact dimensions.
        QTimer::singleShot(200,&w,[&w]{w.resize(1110,690);});
        QTRY_COMPARE_WITH_TIMEOUT(l.setting("windowWidth").toInt(),1110,3000);
        QTRY_COMPARE_WITH_TIMEOUT(l.setting("windowHeight").toInt(),690,3000);
        w.resize(1130,710);
        // Closing must also flush a resize whose debounce has not fired yet.
        QVERIFY(w.close());
        QCOMPARE(l.setting("windowWidth").toInt(),1130);
        QCOMPARE(l.setting("windowHeight").toInt(),710);
        QVERIFY(!l.setting("windowMaximized").toBool());
    }
    void lyricsExactCandidateAndCancel(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"原创测试甲","a"),b=seed(l,"原创测试乙","b"),c=seed(l,"原创测试丙","c");FixtureServer server;QVERIFY(server.listen(QHostAddress::LocalHost));server.reply=[&](const QUrl &url){const QUrlQuery query(url);if(query.queryItemValue("track_name")==a.title)return qMakePair(200,QJsonDocument(lyricRecord(a)).toJson());if(url.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));auto alternate=lyricRecord(b);alternate["artistName"]="另一个歌手";alternate["duration"]=32;return qMakePair(200,QJsonDocument(QJsonArray{alternate}).toJson());};
        {LyricsLookup dialog(&l,{a.id},nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(!l.track(a.id).lyrics.isEmpty(),5000);QVERIFY(QFileInfo::exists(l.absolute(l.track(a.id).lyrics)));QVERIFY(server.requests>=3&&server.requests<=5);QVERIFY(LyricsLookup::confident(a,lyricRecord(a)));dialog.close();}
        {LyricsLookup dialog(&l,{b.id},nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);auto *tasks=dialog.findChild<QListWidget *>("lyricsTracks");QTRY_VERIFY_WITH_TIMEOUT(tasks->item(0)->data(Qt::UserRole+2).toString().startsWith("需选择"),5000);QCOMPARE(dialog.findChild<QListWidget *>("lyricsVersions")->count(),0);tasks->setCurrentRow(0);QTRY_COMPARE(dialog.findChild<QListWidget *>("lyricsVersions")->count(),1);QVERIFY(l.track(b.id).lyrics.isEmpty());capture(dialog,"lyrics-candidates.png");dialog.findChild<QPushButton *>("lyricsApply")->click();QVERIFY(!l.track(b.id).lyrics.isEmpty());dialog.close();}
        server.hold=true;const int before=server.requests;{LyricsLookup dialog(&l,{c.id},nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(server.requests>before,5000);dialog.findChild<QPushButton *>("lyricsStop")->click();QTest::qWait(150);QVERIFY(l.track(c.id).lyrics.isEmpty());QVERIFY(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled());dialog.close();}
    }
    void roundedRowAndAllMenuCorners(){
        SongTable table;table.resize(1100,230);Track t;t.id=1;t.title="圆角测试";table.setSongs({t},{},1);table.show();table.selectRow(0);QTest::qWait(30);const auto rect=table.visualItemRect(table.item(0,SongTable::NumberColumn));const auto image=table.viewport()->grab().toImage();const int dpr=image.devicePixelRatio();const auto highlight=table.palette().color(QPalette::Highlight);QVERIFY(image.pixelColor(1,(rect.y()+1)*dpr).rgb()!=highlight.rgb());QCOMPARE(image.pixelColor(22*dpr,(rect.y()+22)*dpr).rgb(),highlight.rgb());
        RoundedMenu menu;menu.addAction("选项一");menu.addAction("选项二");menu.popup(QPoint(40,40));QTest::qWait(40);const auto snapshot=menu.grab().toImage();QVERIFY(snapshot.pixelColor(0,0).alpha()<40);QVERIFY(snapshot.pixelColor(snapshot.width()-1,snapshot.height()-1).alpha()<40);capture(menu,"rounded-menu.png");menu.close();
    }
};
QTEST_MAIN(V14Tests)
#include "v14-tests.moc"