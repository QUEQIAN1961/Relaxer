#include "c4c5-ui-fixture.h"
#include "changelog-summary.h"
#include "changelog.h"
#include "window.h"
#include <QJsonDocument>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
using namespace c4c5_fixture;
namespace {
bool click(QQuickWindow *w,QQuickItem *item) {
    if(!item||!item->isVisible()||!item->isEnabled())return false;
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
    return true;
}
}
class Rc6UserRegressions : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS","1");}
    void instrumentalPlaceholders_data(){
        QTest::addColumn<QByteArray>("input");
        QTest::newRow("plain")<<QStringLiteral("此歌曲为没有填词的纯音乐，请您欣赏").toUtf8();
        QTest::newRow("timed-DJ")<<QStringLiteral("[ar:DJ]\n[00:00.00]此歌曲为没有填词的DJ舞曲，请您欣赏\n[00:04.00]\n").toUtf8();
        QTest::newRow("repeated")<<QStringLiteral("[00:00]此歌曲为没有填词的纯音乐，请您欣赏\n[00:10]此歌曲为没有填词的纯音乐，请您欣赏\n").toUtf8();
        QTest::newRow("canonical")<<QStringLiteral("纯音乐，请欣赏").toUtf8();
    }
    void instrumentalPlaceholders(){
        QFETCH(QByteArray,input);const auto parsed=Lyrics::parse(input);
        QVERIFY(parsed.instrumental);QVERIFY(!parsed.timed);QCOMPARE(parsed.lines.size(),1);
        QCOMPARE(parsed.lines[0].text,QStringLiteral("纯音乐，请欣赏"));
        Fixture f(2);QVERIFY(f.ok);f.shell->playbackBridge()->pause();
        // Simulate a pre-existing LRC whose old persisted flag was false.
        QVERIFY(!f.library.isInstrumental(f.a.id));
        QFile file(f.library.absolute(f.library.track(f.a.id).lyrics));QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));
        QCOMPARE(file.write(input),input.size());file.close();
        f.shell->lyricsModel()->reload();QTRY_COMPARE(f.shell->lyricsModel()->status(),QStringLiteral("纯音乐，请欣赏"));
        QCOMPARE(f.shell->lyricsModel()->count(),1);
        QCOMPARE(f.shell->lyricsModel()->data(f.shell->lyricsModel()->index(0),LyricsModel::Original).toString(),QStringLiteral("纯音乐，请欣赏"));
        QVERIFY(!f.library.isInstrumental(f.a.id)); // Display does not rewrite user data.
    }
    void realLyricsRemainLyrics(){
        for(const auto &bytes:{QByteArray("[00:01]DJ dance remix\n[00:02]Actual words\n"),
            QStringLiteral("[00:01]此歌曲为没有填词的纯音乐，请您欣赏\n[00:02]真实歌词\n").toUtf8(),
            QStringLiteral("我在听纯音乐，请欣赏这段歌声").toUtf8()}){
            const auto parsed=Lyrics::parse(bytes);QVERIFY(!parsed.instrumental);QVERIFY(!parsed.lines.isEmpty());
        }
    }
    void changelogGroupsPreserveHistoryAndGa(){
        QFile file(":/changelog.json");QVERIFY(file.open(QIODevice::ReadOnly));const auto original=QJsonDocument::fromJson(file.readAll()).array();
        const auto summary=summarizedChangelog(original);int rc020=0,rc022=0;
        for(const auto &value:summary){const auto row=value.toArray();const auto title=row[0].toString();
            if(title=="V0.2.0-RC")++rc020;if(title=="V0.2.2-RC")++rc022;
            QVERIFY(!QRegularExpression("V0\\.2\\.[02]-[Rr][Cc][0-9]+").match(title).hasMatch());
        }
        QCOMPARE(rc020,1);QCOMPARE(rc022,1);
        for(const auto &value:original){const auto row=value.toArray();const auto title=row[0].toString();
            if(title=="Relaxer V0.2.0-GA")QVERIFY(summary.contains(value));
            if(QRegularExpression("V0\\.2\\.[02]-[Rr][Cc][0-9]+").match(title).hasMatch()){
                bool kept=false;for(const auto &merged:summary)if(merged.toArray()[2].toString().contains(row[2].toString()))kept=true;
                QVERIFY2(kept,qPrintable(title));
            }
        }
        Fixture f(2);QVERIFY(f.ok);QCOMPARE(f.shell->settingsBridge()->changelog(),summary.toVariantList());
    }
    void versionLogReentryAndPlayerReturn(){
        Fixture f(2);QVERIFY(f.ok);auto *w=f.shell->window();auto *root=w->contentItem();
        f.shell->appBridge()->navigate(3);w->setProperty("settingsSection","log");
        auto *page=visual(root,"versionLogPage");QVERIFY(page);QTRY_VERIFY(page->isVisible());QCOMPARE(page->property("shown").toInt(),3);
        QVERIFY(QMetaObject::invokeMethod(visual(root,"versionLogExpand"),"clicked"));QTRY_VERIFY(page->property("shown").toInt()>3);
        QVERIFY(click(w,visual(root,"versionLogBack")));QTRY_COMPARE(w->property("settingsSection").toString(),QString("settings"));
        w->setProperty("settingsSection","log");QTRY_VERIFY(page->isVisible());QCOMPARE(page->property("shown").toInt(),3);
        page->setProperty("shown",12);f.shell->appBridge()->navigate(0);f.shell->appBridge()->navigate(3);
        QTRY_VERIFY(page->isVisible());QCOMPARE(page->property("shown").toInt(),3);
        QVERIFY(!visual(root,"playerFullscreen"));QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        QVERIFY(click(w,visual(root,"playerCollapse")));QTRY_VERIFY(!w->property("lyricsOpen").toBool());
        QVERIFY(f.shell->warnings().isEmpty());
    }
    void widgetsVersionLogReentry(){
        QTemporaryDir d;Library lib(d.filePath("data"));QVERIFY(lib.open());Window window(&lib);window.show();window.showVersionLog();
        auto *log=window.findChild<ChangeLog *>();QVERIFY(log);QCOMPARE(log->visibleCount(),3);
        window.findChild<QPushButton *>("versionLogExpand")->click();QVERIFY(log->visibleCount()>3);
        window.findChild<QPushButton *>("versionLogBack")->click();window.showVersionLog();QCOMPARE(log->visibleCount(),3);
    }
    void feedbackHistoryDismissalLifecycle(){
        Fixture f(2);QVERIFY(f.ok);auto *w=f.shell->window();auto *root=w->contentItem();auto *service=f.shell->feedbackBridge();
        auto *history=visual(root,"feedbackHistoryButton");auto *dismiss=visual(root,"feedbackHistoryDismiss");
        auto *details=w->findChild<QObject *>("feedbackDetailsDialog");auto *timer=w->findChild<QObject *>("feedbackHistoryExpiry");
        QVERIFY(history&&dismiss&&details&&timer);
        auto event=[&]{service->post("Lyrics","Information","查找歌词完成");service->dismiss();};
        event();QTRY_VERIFY(history->isVisible());QVERIFY(click(w,dismiss));QTRY_VERIFY(!history->isVisible());
        QCOMPARE(service->history().size(),1);
        event();QTRY_VERIFY(history->isVisible());QVERIFY(click(w,history));QTRY_VERIFY(details->property("opened").toBool());
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!details->property("visible").toBool());QVERIFY(!history->isVisible());
        event();QTRY_VERIFY(history->isVisible());QVERIFY(click(w,history));QTRY_VERIFY(details->property("opened").toBool());
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(50,50));QTRY_VERIFY(!details->property("visible").toBool());QVERIFY(!history->isVisible());
        event();QTRY_VERIFY(history->isVisible());QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(500,100));QTRY_VERIFY(!history->isVisible());
        event();QTRY_VERIFY(history->isVisible());timer->setProperty("interval",80);QVERIFY(QMetaObject::invokeMethod(timer,"restart"));QTRY_VERIFY(!history->isVisible());
        QCOMPARE(service->history().size(),5);QVERIFY(f.shell->warnings().isEmpty());
    }
};
QTEST_MAIN(Rc6UserRegressions)
#include "rc6-user-regressions.moc"
