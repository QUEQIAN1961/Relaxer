#include "window.h"
#include "release-version.h"
#include <QAbstractItemView>
#include <QAction>
#include <QFontDatabase>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace relaxer;
class V12Tests:public QObject{
    Q_OBJECT
    static void capture(QWidget *widget,const QString &file){auto dir=qEnvironmentVariable("RELAXER_QA_DIR");if(!dir.isEmpty()){QVERIFY(QDir().mkpath(dir));QVERIFY(widget->grab().save(dir+"/"+file));}}
private slots:
    void initTestCase(){qApp->setStyle("Fusion");auto f=qEnvironmentVariable("RELAXER_QA_FONT");if(!f.isEmpty())QVERIFY(QFontDatabase::addApplicationFont(f)>=0);}
    void integratedChrome(){QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(30);QVERIFY(w.findChild<TitleBar *>("windowControls"));QCOMPARE(w.windowTitle(),QString::fromLatin1(release::WindowTitle));
        QVERIFY(w.windowFlags().testFlag(Qt::FramelessWindowHint));auto *max=w.findChild<CaptionButton *>("windowMaximize");QVERIFY(max);max->click();QTRY_VERIFY(w.isMaximized());max->click();QTRY_VERIFY(!w.isMaximized());QTest::mouseDClick(w.findChild<QWidget *>("appBrand"),Qt::LeftButton);QTRY_VERIFY(w.isMaximized());w.showNormal();w.findChild<CaptionButton *>("windowMinimize")->click();QTRY_VERIFY(w.isMinimized());w.showNormal();w.findChild<CaptionButton *>("windowClose")->click();QTRY_VERIFY(!w.isVisible());
    }
    void appearanceKeepsCurrentLook_data(){QTest::addColumn<int>("theme");QTest::newRow("light")<<0;QTest::newRow("dark-over-old-light-custom")<<1;QTest::newRow("system")<<2;QTest::newRow("custom-image")<<3;}
    void appearanceKeepsCurrentLook(){
        QFETCH(int,theme);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());AppearanceStore store(&l);auto old=AppearanceStore::defaults();
        if(theme==3){QImage image(80,80,QImage::Format_RGB32);image.fill(QColor("#264d55"));const auto path=d.filePath("bg.png");QVERIFY(image.save(path));old["mode"]="image";old["shareLR"]=true;old["sourceLeft"]=path;}
        QVERIFY(store.save(old,{}));QVERIFY(l.setSetting("theme",theme));const auto history=store.state();Window w(&l);
        if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);
        w.show();w.showSettings();QTest::qWait(300);const auto palette=qApp->palette();
        w.findChild<QPushButton *>("editAppearance")->click();auto *page=w.findChild<AppearancePage *>();QVERIFY(page);QTest::qWait(300);
        QVERIFY(!page->isWindow());QCOMPARE(qApp->palette(),palette);
        if(theme==1)QVERIFY(page->findChild<QPushButton *>("appearanceColor-right")->text().contains("#19191e"));
        if(theme==3)QVERIFY(!page->findChild<QLineEdit *>("appearanceImageLeft")->text().isEmpty());
        capture(page,QString("23-editor-theme-%1.png").arg(theme));page->findChild<QPushButton *>("appearanceBack")->click();
        QCOMPARE(qApp->palette(),palette);QCOMPARE(l.setting("theme").toInt(),theme);QCOMPARE(store.state(),history);
    }
    void darkDraftSavedAndRestored(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());l.setSetting("theme",1);
        { Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();w.showSettings();
          w.findChild<QPushButton *>("editAppearance")->click();auto *page=w.findChild<AppearancePage *>();QVERIFY(page);
          page->findChild<QPushButton *>("appearanceMode3")->click();page->findChild<QPushButton *>("appearanceNewSave")->click();
          QCOMPARE(l.setting("theme").toInt(),3);QCOMPARE(AppearanceStore(&l).active()["right"].toString(),QString("#19191e")); }
        Window reopened(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))reopened.playback()->media()->setAudioOutput(nullptr);QCOMPARE(qApp->palette().color(QPalette::Window),QColor("#19191e"));
    }
    void versionLogNavigation(){QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();w.showSettings();QTest::qWait(30);auto *scroll=w.findChild<QScrollArea *>("settingsScroll");scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());const int position=scroll->verticalScrollBar()->value();auto volume=w.playback()->volume();w.showVersionLog();QCOMPARE(w.findChild<QStackedWidget *>("pages")->currentWidget()->objectName(),QString("versionLogPage"));QCOMPARE(w.findChildren<QLabel *>("versionLogTitle").size(),3);QVERIFY(w.findChild<QLabel *>("copyright")->text().contains("All rights reserved."));QTest::keyClick(&w,Qt::Key_Escape);QCOMPARE(w.findChild<QStackedWidget *>("pages")->currentWidget()->objectName(),QString("settingsPage"));QVERIFY(w.findChild<QStackedWidget *>("pages")->currentWidget()->isAncestorOf(scroll));QCOMPARE(scroll->verticalScrollBar()->value(),position);w.showVersionLog();w.findChild<QPushButton *>("versionLogBack")->click();QCOMPARE(scroll->verticalScrollBar()->value(),position);QCOMPARE(w.playback()->volume(),volume);QTest::qWait(20);scroll->verticalScrollBar()->setValue(0);QTest::qWait(20);capture(&w,"24-settings-light.png");}
    void actualPlayerButtonStaysRound_data(){QTest::addColumn<int>("theme");QTest::newRow("light")<<0;QTest::newRow("dark")<<1;}
    void actualPlayerButtonStaysRound(){QFETCH(int,theme);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());l.setSetting("theme",theme);Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();auto *button=w.findChild<CircleButton *>("playButton");QVERIFY(button);for(int width:{1000,1240,1600}){w.resize(width,790);button->clearFocus();QTest::qWait(30);QCOMPARE(button->size(),QSize(42,42));auto im=button->grab().toImage();auto accent=qApp->palette().color(QPalette::Highlight);QRect colored;for(int y=0;y<im.height();++y)for(int x=0;x<im.width();++x){auto c=im.pixelColor(x,y);if(qAbs(c.red()-accent.red())+qAbs(c.green()-accent.green())+qAbs(c.blue()-accent.blue())<12)colored=colored.united(QRect(x,y,1,1));}QVERIFY(!colored.isEmpty());QVERIFY(qAbs(colored.width()-colored.height())<=1);QVERIFY(colored.width()/im.devicePixelRatio()>37);}CircleButton isolated("play","test");isolated.show();QSignalSpy spy(&isolated,&QAbstractButton::clicked);QTest::mouseClick(&isolated,Qt::LeftButton,Qt::NoModifier,QPoint(1,1));QCOMPARE(spy.size(),0);QTest::mouseClick(&isolated,Qt::LeftButton,Qt::NoModifier,QPoint(21,21));QCOMPARE(spy.size(),1);w.showSettings();QTest::qWait(20);capture(&w,QString("20-settings-%1.png").arg(theme==1?"dark":"light"));}
    void roundedDropdownAndMenu_data(){QTest::addColumn<int>("theme");QTest::newRow("light")<<0;QTest::newRow("dark")<<1;}
    void roundedDropdownAndMenu(){QFETCH(int,theme);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());l.setSetting("theme",theme);Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();w.showSettings();QTest::qWait(260);auto *combo=w.findChild<RoundedComboBox *>("closeSetting");QVERIFY(combo);combo->showPopup();QTest::qWait(100);auto *popup=combo->view()->window();QVERIFY(popup->isVisible());QVERIFY(popup->testAttribute(Qt::WA_TranslucentBackground));auto image=popup->grab().toImage();QVERIFY(image.pixelColor(0,0).alpha()<40);QCOMPARE(image.pixelColor(image.width()/2,3).rgb(),qApp->palette().color(QPalette::Button).rgb());capture(popup,QString("18-dropdown-%1.png").arg(theme==1?"dark":"light"));auto item=combo->view()->visualRect(combo->model()->index(1,0));QVERIFY(combo->view()->viewport()->rect().contains(item.center()));QTest::mouseMove(combo->view()->viewport(),item.center());QTest::mouseClick(combo->view()->viewport(),Qt::LeftButton,Qt::NoModifier,item.center());QTRY_COMPARE(combo->currentIndex(),1);QVERIFY(l.setting("closeToTray").toBool());combo->showPopup();QTest::qWait(30);QTest::keyClick(combo->view(),Qt::Key_Up);QTest::keyClick(combo->view(),Qt::Key_Return);QTRY_COMPARE(combo->currentIndex(),0);QVERIFY(!l.setting("closeToTray").toBool());RoundedMenu menu(&w);auto *action=menu.addAction("菜单项");menu.popup(w.mapToGlobal(QPoint(350,250)));QTest::qWait(30);QVERIFY(menu.grab().toImage().pixelColor(0,0).alpha()<40);capture(&menu,QString("19-menu-%1.png").arg(theme==1?"dark":"light"));QSignalSpy triggered(action,&QAction::triggered);QTest::mouseClick(&menu,Qt::LeftButton,Qt::NoModifier,menu.actionGeometry(action).center());QCOMPARE(triggered.size(),1);}
    void roundedLongListScrolls(){RoundedComboBox combo;for(int i=0;i<16;++i)combo.addItem(QString("主题 %1").arg(i));combo.setMaxVisibleItems(4);combo.resize(220,40);combo.show();combo.showPopup();QTest::qWait(40);auto *view=combo.view();auto last=combo.model()->index(15,0);view->scrollTo(last);QTest::qWait(20);auto item=view->visualRect(last);QVERIFY(view->viewport()->rect().contains(item.center()));QTest::mouseMove(view->viewport(),item.center());QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,item.center());QTRY_COMPARE(combo.currentIndex(),15);}
};
QTEST_MAIN(V12Tests)
#include "v12-tests.moc"
