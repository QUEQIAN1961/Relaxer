#include "c4c5-ui-fixture.h"
#include "changelog-summary.h"
#include "changelog.h"
#include "window.h"
#include <QJsonDocument>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QDate>
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
    void userFacingChangelogCopyAndDates(){
        QFile resource(":/changelog.json");QVERIFY(resource.open(QIODevice::ReadOnly));
        const auto history=QJsonDocument::fromJson(resource.readAll()).array();
        const auto summary=summarizedChangelog(history);
        const QRegularExpression internal(QStringLiteral("私人测试|非\\s*GA|不是\\s*GA|不得公开分发|等待[^。\\n]*验收|尚未正式发布|ART[- ]?[0-9]+|UNRESOLVED|NOT REPRODUCED"),QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression rcInternal(QStringLiteral("Harness|CTest|SKIP|PASS|内部测试|测试候选|发布候选|本版为候选"),QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression date(QStringLiteral("^[0-9]{4}\\.[0-9]{2}\\.[0-9]{2}$"));
        QVERIFY(rcInternal.match(QStringLiteral("私人测试候选")).hasMatch());
        QVERIFY(!rcInternal.match(QStringLiteral("停止歌词搜索时的候选结果")).hasMatch());
        QString rc022,rc020;int groups022=0,groups020=0;
        for(const auto &value:summary){
            const auto row=value.toArray();QCOMPARE(row.size(),3);
            const auto title=row[0].toString(),text=row[2].toString(),stamp=row[1].toString();
            QVERIFY(date.match(stamp).hasMatch());QVERIFY(QDate::fromString(stamp,"yyyy.MM.dd").isValid());
            QVERIFY2(!internal.match(text).hasMatch(),qPrintable(title+": "+text));
            if(title=="V0.2.2-RC"){rc022=text;++groups022;}
            if(title=="V0.2.0-RC"){rc020=text;++groups020;}
            // Published GA history is intentionally preserved, including its
            // historical acceptance sentence; RC notes contain no such wording.
            if(title.endsWith("-RC"))QVERIFY2(!rcInternal.match(text).hasMatch(),qPrintable(title+": "+text));
        }
        QCOMPARE(groups022,1);QCOMPARE(groups020,1);
        for(const auto &term:{"便携 EXE","操作记录","纯音乐，请欣赏","默认折叠","停止歌词搜索","每次完整文件校验","来源核对"})QVERIFY2(rc022.contains(QString::fromUtf8(term)),term);
        for(const auto &term:{"MIDI","无需翻译","未保存译文","付费确认"})QVERIFY2(rc020.contains(QString::fromUtf8(term)),term);
        for(const auto &value:history){const auto title=value.toArray()[0].toString();if(title=="Relaxer V0.2.0-GA"||title=="Relaxer V0.2.1-GA")QVERIFY(summary.contains(value));}
        Fixture f(2);QVERIFY(f.ok);auto *w=f.shell->window();
        f.shell->appBridge()->navigate(3);w->setProperty("settingsSection","log");
        auto *page=visual(w->contentItem(),"versionLogPage");QVERIFY(page);QTRY_VERIFY(page->isVisible());
        QCOMPARE(page->property("shown").toInt(),3);page->setProperty("shown",int(summary.size()));
        std::function<QStringList(QQuickItem *)> texts=[&](QQuickItem *item){
            QStringList result;if(item->isVisible()&&item->property("text").isValid())result.append(item->property("text").toString());
            for(auto *child:item->childItems())result.append(texts(child));return result;
        };
        QTRY_VERIFY(texts(page).contains(rc022));QTRY_VERIFY(texts(page).contains(rc020));
        QCOMPARE(f.shell->settingsBridge()->changelog(),summary.toVariantList());
        const QString output=qEnvironmentVariable("RELAXER_TEXT_FIX_EVIDENCE");
        if(!output.isEmpty()){
            QVERIFY(QDir().mkpath(output));QFile file(QDir(output).filePath("user-visible-changelog.json"));QVERIFY(file.open(QIODevice::WriteOnly));
            const auto bytes=QJsonDocument(summary).toJson(QJsonDocument::Indented);QCOMPARE(file.write(bytes),bytes.size());file.close();
            QTest::qWait(100);const auto image=w->grabWindow();QVERIFY(!image.isNull());QVERIFY(image.save(QDir(output).filePath("qml-version-log.png")));
            QFile actual(QDir(output).filePath("qml-rendered-text.json"));QVERIFY(actual.open(QIODevice::WriteOnly));
            actual.write(QJsonDocument(QJsonArray::fromStringList(texts(page))).toJson());
        }
        QTemporaryDir d;Library lib(d.filePath("data"));QVERIFY(lib.open());Window window(&lib);window.show();window.showVersionLog();
        auto *log=window.findChild<ChangeLog *>();QVERIFY(log);QCOMPARE(log->visibleCount(),3);
        while(log->visibleCount()<summary.size()){auto *expand=window.findChild<QPushButton *>("versionLogExpand");QVERIFY(expand);expand->click();}
        QStringList widgetText;for(auto *label:log->findChildren<QLabel *>())widgetText.append(label->text());
        QVERIFY(widgetText.contains(rc022));QVERIFY(widgetText.contains(rc020));
        if(!output.isEmpty()){QTRY_VERIFY(log->isVisible());QTest::qWait(600);QVERIFY(window.grab().save(QDir(output).filePath("widgets-version-log.png")));}
        QVERIFY(f.shell->warnings().isEmpty());
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
