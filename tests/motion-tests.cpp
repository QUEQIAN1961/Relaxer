#include "window.h"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
class MotionTests:public QObject {
    Q_OBJECT
    static void capture(Window &w,const QString &name){const auto dir=qEnvironmentVariable("RELAXER_QA_DIR");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.grab().save(dir+"/"+name));}}
    static void seed(Library &l){
        QFile f(l.absolute("Music/motion.wav"));QVERIFY(f.open(QIODevice::WriteOnly));QDataStream out(&f);out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF",4);out<<quint32(16036);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(16000);out.writeRawData(QByteArray(16000,'\0').constData(),16000);f.close();
        QImage cover(500,500,QImage::Format_RGB32);cover.fill(QColor("#dd6688"));QPainter painter(&cover);painter.fillRect(45,45,410,410,QColor("#674d90"));painter.setPen(Qt::white);QFont font;font.setPixelSize(40);painter.setFont(font);painter.drawText(cover.rect(),Qt::AlignCenter,"RELAXER\nMOTION");painter.end();QVERIFY(cover.save(l.absolute("Artwork/motion.png")));
        Track t;t.hash="motion";t.file="Music/motion.wav";t.title="音乐与时光";t.artist="Relaxer";t.album="慢慢听";t.cover="Artwork/motion.png";t.coverOrigin="custom";t.duration=1000;t.id=l.insert(t);QVERIFY(t.id);
        const auto id=l.createPlaylist("动效测试");QVERIFY(l.addToPlaylist(id,{t.id}));QVERIFY(l.favorite(t.id,true));
        l.setSetting("volume",0);l.setSetting("queue",QJsonArray{double(t.id)});l.setSetting("current",double(t.id));
    }
    static void playlist(Window &w){auto *lists=w.findChild<QListWidget *>("playlistNavigation");QTest::mouseClick(lists->viewport(),Qt::LeftButton,Qt::NoModifier,lists->visualItemRect(lists->item(0)).center());}
private slots:
    void initTestCase(){qApp->setStyle("Fusion");}
    void toolbarAndPageReadability(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());seed(l);Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(70);
        auto *search=w.findChild<QLineEdit *>("search");const int anchor=search->mapTo(&w,QPoint()).y();playlist(w);QTest::qWait(260);QCOMPARE(search->mapTo(&w,QPoint()).y(),anchor);capture(w,"01-playlist-aligned.png");
        auto *nav=w.findChild<QListWidget *>("navigation");QTest::mouseClick(nav->viewport(),Qt::LeftButton,Qt::NoModifier,nav->visualItemRect(nav->item(4)).center());QTest::qWait(260);QCOMPARE(search->mapTo(&w,QPoint()).y(),anchor);capture(w,"02-favorites-anchor.png");
        w.showSettings();QTest::qWait(45);capture(w,"03-page-recess.png");QTest::qWait(85);capture(w,"04-page-enter.png");QTest::qWait(160);QVERIFY(!static_cast<PageMotion *>(w.findChild<QWidget *>("pageMotion"))->isVisible());
        w.showLibrary();w.showPlaylists();w.showProfile();w.showSettings();QTest::qWait(270);QCOMPARE(w.findChild<QStackedWidget *>("pages")->currentIndex(),3);QVERIFY(!static_cast<PageMotion *>(w.findChild<QWidget *>("pageMotion"))->isVisible());
    }
    void sharedCoverReturnAndInterruption(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());seed(l);Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(70);
        auto *bottom=w.findChild<Cover *>("bottomCover"),*large=w.findChild<Cover *>("nowCover");auto *motion=static_cast<PageMotion *>(w.findChild<QWidget *>("pageMotion"));auto *controls=w.findChild<QWidget *>("playerCenter");const QRect geometry=controls->geometry();
        w.showNowPlaying();QTest::qWait(90);QVERIFY(motion->isVisible());QVERIFY(!bottom->isVisible());QVERIFY(!large->isVisible());capture(w,"05-cover-rising.png");
        QTest::qWait(160);capture(w,"06-cover-arriving.png");QTest::qWait(240);QVERIFY(!motion->isVisible());QVERIFY(large->isVisible());QVERIFY(!bottom->isVisible());QCOMPARE(controls->geometry(),geometry);capture(w,"07-player-expanded.png");
        w.showLibrary();QTest::qWait(90);capture(w,"08-cover-returning.png");QTRY_VERIFY_WITH_TIMEOUT(bottom->isVisible(),1500);QVERIFY(!motion->isVisible());QCOMPARE(controls->geometry(),geometry);
        w.showNowPlaying();QTest::qWait(70);const auto position=motion->currentArtwork().coverFrom;w.showLibrary();QTest::qWait(3);const auto reversed=motion->currentArtwork().coverFrom;QVERIFY(QLineF(position.center(),reversed.center()).length()<35);QTRY_VERIFY_WITH_TIMEOUT(bottom->isVisible(),1500);QVERIFY(!motion->isVisible());
        w.showNowPlaying();QTest::qWait(60);w.resize(1340,840);QTest::qWait(50);QVERIFY(!motion->isVisible());QVERIFY(large->isVisible());QVERIFY(!bottom->isVisible());w.showLibrary();QTRY_VERIFY_WITH_TIMEOUT(bottom->isVisible(),1200);
    }
};
QTEST_MAIN(MotionTests)
#include "motion-tests.moc"