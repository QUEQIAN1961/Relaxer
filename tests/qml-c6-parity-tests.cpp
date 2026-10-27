#include "c4c5-ui-fixture.h"
#include "shortcut-rules.h"
#include "ui-bridge/ui-integration.h"
#include <QAction>
#include <QApplication>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSemaphore>
#include <QWidget>
#include <QUuid>
#include <thread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace c4c5_fixture;
namespace {
QString evidence;
int sequence=0;
void record(const QString &name,QJsonObject data){
    QFile file(QDir(evidence).filePath(QString("%1-%2.json").arg(++sequence,3,10,QChar('0')).arg(name)));
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))qFatal("Cannot preserve native parity evidence");
    file.write(QJsonDocument(data).toJson());
}
bool prepare(Fixture &fixture){
    if(!fixture.ok)return false;
    auto *window=fixture.shell->window();window->showNormal();window->setGeometry(45,50,1240,790);window->requestActivate();
    if(!QTest::qWaitForWindowExposed(window))return false;
    fixture.shell->playbackBridge()->pause();QTest::qWait(100);return true;
}
QQuickItem *textItem(QQuickItem *root,const QString &text){
    if(root->property("text").toString()==text)return root;
    for(auto *child:root->childItems())if(auto *result=textItem(child,text))return result;
    return nullptr;
}
QString family(QQuickItem *item){return item?item->property("font").value<QFont>().family():QString();}
QMenu *trayMenu(){
    for(auto *widget:QApplication::allWidgets())if(auto *menu=qobject_cast<QMenu *>(widget))
        for(auto *action:menu->actions())if(action->text()=="打开 Relaxer")return menu;
    return nullptr;
}
QAction *action(QMenu *menu,const QString &text){for(auto *entry:menu->actions())if(entry->text()==text)return entry;return nullptr;}
#ifdef Q_OS_WIN
// Reserve from another OS thread: verifies the actual Windows registry,
// including conflicts and lifecycle release, without touching another app.
class Reservation {
    QSemaphore ready,finish;std::thread thread;
public:
    bool registered=false;
    explicit Reservation(UINT key){thread=std::thread([this,key]{
        registered=RegisterHotKey(nullptr,0x4f00,MOD_CONTROL|MOD_ALT|MOD_SHIFT,key);
        ready.release();finish.acquire();if(registered)UnregisterHotKey(nullptr,0x4f00);
    });ready.acquire();}
    ~Reservation(){finish.release();thread.join();}
};
bool pressGlobal(){
    INPUT events[8]{};const WORD keys[]{VK_CONTROL,VK_MENU,VK_SHIFT,VK_F24,VK_F24,VK_SHIFT,VK_MENU,VK_CONTROL};
    for(int i=0;i<8;++i){events[i].type=INPUT_KEYBOARD;events[i].ki.wVk=keys[i];events[i].ki.dwFlags=i>=4?KEYEVENTF_KEYUP:0;}
    const auto sent=SendInput(8,events,sizeof(INPUT));
    if(sent!=8){INPUT releases[4]{};for(int i=0;i<4;++i){releases[i].type=INPUT_KEYBOARD;releases[i].ki.wVk=keys[3-i];releases[i].ki.dwFlags=KEYEVENTF_KEYUP;}SendInput(4,releases,sizeof(INPUT));}
    return sent==8;
}
#endif
}
class C6ParityTests final:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){
        const auto root=qEnvironmentVariable("RELAXER_C6_EVIDENCE",QCoreApplication::applicationDirPath()+"/Evidence/C6-B");
        evidence=root+"/Parity-"+QUuid::createUuid().toString(QUuid::Id128);QVERIFY(QDir().mkpath(evidence));
        qInfo().noquote()<<"C6_PARITY_EVIDENCE"<<evidence;
    }
    void nativeAppAndGlobalShortcutRegistrationDispatchAndRelease(){
        Fixture fixture(2);QVERIFY(prepare(fixture));auto *window=fixture.shell->window();
        auto *integration=fixture.shell->findChild<UiIntegration *>();QVERIFY(integration);
        fixture.shell->playbackBridge()->setVolume(40);window->contentItem()->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Up,Qt::ControlModifier);QCOMPARE(fixture.shell->playbackBridge()->volume(),45);
        auto *search=visual(window->contentItem(),"librarySearch");QVERIFY(search);search->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Up,Qt::ControlModifier);QCOMPARE(fixture.shell->playbackBridge()->volume(),45);
        window->contentItem()->forceActiveFocus();
#ifdef Q_OS_WIN
        auto config=ShortcutRules::defaults();auto global=config["global"].toObject();
        for(const auto &key:ShortcutRules::actionKeys())global[key]="";
        global["volumeUp"]="Ctrl+Alt+Shift+F24";config["global"]=global;config["globalEnabled"]=true;
        QVERIFY(integration->saveShortcuts(config.toVariantMap()));
        {Reservation busy(VK_F24);QVERIFY(!busy.registered);}
        {
            Reservation occupied(VK_F22);QVERIFY(occupied.registered);
            auto rejected=config;auto keys=global;keys["volumeUp"]="Ctrl+Alt+Shift+F22";rejected["global"]=keys;
            QVERIFY(!integration->saveShortcuts(rejected.toVariantMap()));
            QCOMPARE(fixture.library.setting("shortcuts").toObject(),config);
        }
        {Reservation rollback(VK_F24);QVERIFY(!rollback.registered);}
        QWidget ownedForeground;ownedForeground.setWindowTitle("Relaxer owned shortcut test");ownedForeground.resize(260,90);
        ownedForeground.show();ownedForeground.raise();ownedForeground.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&ownedForeground));
        const bool interactive=GetForegroundWindow()!=nullptr;
        UINT registeredId=0;
        if(interactive){
            QCOMPARE(GetForegroundWindow(),reinterpret_cast<HWND>(ownedForeground.winId()));
            QVERIFY(pressGlobal());
        }else{
            // A service/isolated desktop can expose Qt windows but have no OS
            // foreground window. Do not inject input into an unknown desktop
            // or claim physical-key dispatch. Verify the registered native
            // handler, then the real OS thread-message queue below.
            MSG message{};message.message=WM_HOTKEY;qintptr result=0;
            for(UINT id=1;id<0xbfff;++id){message.wParam=id;
                if(integration->nativeEventFilter("windows_dispatcher_MSG",&message,&result)){registeredId=id;break;}}
            QVERIFY(registeredId);
        }
        QTRY_COMPARE(fixture.shell->playbackBridge()->volume(),50);
        integration->setShortcutRecording(true);
        {Reservation released(VK_F24);QVERIFY(released.registered);}
        integration->setShortcutRecording(false);
        if(interactive)QVERIFY(pressGlobal());
        else QVERIFY(PostThreadMessageW(GetCurrentThreadId(),WM_HOTKEY,registeredId,MAKELPARAM(MOD_CONTROL|MOD_ALT|MOD_SHIFT,VK_F24)));
        QTRY_COMPARE(fixture.shell->playbackBridge()->volume(),55);
        fixture.shell.reset();
        {Reservation destroyed(VK_F24);QVERIFY(destroyed.registered);}
        record("native-global-shortcuts",{{"osRegistration",true},{"conflictRollback",true},{"ownedOtherWindowForeground",interactive},
            {"osSendInputDispatch",interactive},{"osKeyboardDispatchStatus",interactive?"EXECUTED":"NOT EXECUTED: OS foreground window unavailable"},
            {"nativeThreadMessageDispatch",!interactive},{"recordingReleasesAndRestores",true},{"destructionReleases",true},{"finalVolume",55}});
#else
        record("native-global-shortcuts",{{"status","NOT EXECUTED: Windows-only registration"}});
#endif
        QCOMPARE(fixture.http.requestCount(),0);
    }
    void trayActionsUseExistingFacadeRestoreAndQuit(){
        Fixture fixture(2);QVERIFY(prepare(fixture));auto *window=fixture.shell->window();auto *facade=fixture.shell->facade();
        auto *menu=trayMenu();QVERIFY(menu);auto *open=action(menu,"打开 Relaxer");auto *toggle=action(menu,"播放 / 暂停");
        auto *next=action(menu,"下一首");auto *previous=action(menu,"上一首");auto *quit=action(menu,"退出播放器");
        QVERIFY(open&&toggle&&next&&previous&&quit);
        const bool hostTray=QSystemTrayIcon::isSystemTrayAvailable();
        QVERIFY(fixture.shell->settingsBridge()->setPreference("closeToTray",true));
        if(hostTray){QVERIFY(!window->close());QTRY_VERIFY(!window->isVisible());}
        else window->hide();
        open->trigger();QTRY_VERIFY(window->isVisible());
        toggle->trigger();QTRY_VERIFY(facade->playing());toggle->trigger();QTRY_VERIFY(!facade->playing());
        next->trigger();QTRY_COMPARE(facade->current().id,fixture.b.id);previous->trigger();QTRY_COMPARE(facade->current().id,fixture.a.id);
        QCOMPARE(fixture.shell->findChildren<Playback *>().size(),1);QCOMPARE(QSqlDatabase::connectionNames().size(),1);
        quit->trigger();QTRY_VERIFY(!window->isVisible());QVERIFY(window->property("exitRequested").toBool());
        record("tray-actions",{{"hostTrayAvailable",hostTray},{"hostCloseToTrayExecuted",hostTray},{"actualQActions",true},
            {"restore",true},{"playPausePreviousNext",true},{"quitBypassesCloseToTray",true},{"playbackInstances",1},{"databaseConnections",1}});
    }
    void allFiveFontRegionsReachActualQmlTextIndependently(){
        Fixture fixture(2);QVERIFY(prepare(fixture));auto *window=fixture.shell->window();auto *bridge=fixture.shell->settingsBridge();
        QVERIFY(fixture.library.saveLyrics(fixture.a.id,"[00:00.00]Hello moon\n[00:00.00]月亮你好\n[00:01.00]Hello world\n[00:01.00]你好世界\n"));
        QVERIFY(bridge->setPreference("showLyricTranslation",true));
        QVERIFY(QMetaObject::invokeMethod(window,"showLyrics"));QTRY_COMPARE(fixture.shell->lyricsModel()->count(),2);
        QTRY_VERIFY(window->property("playerProgress").toDouble()>.999);
        const QString source=qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/segoeui.ttf";QVERIFY(QFileInfo::exists(source));
        QVariantMap flags;const QStringList regions{"songs","lyrics","footer","next","rest"};for(const auto &region:regions)flags[region]=false;
        QVERIFY(bridge->saveFont(QUrl::fromLocalFile(source),flags));
        auto *list=visual(window->contentItem(),"trackListView");auto *lyrics=visual(window->contentItem(),"lyricsList");
        auto *upcoming=visual(window->contentItem(),"nextQueuePanel");QVERIFY(list&&lyrics&&upcoming);
        QTRY_VERIFY(textItem(list,fixture.a.title));QTRY_VERIFY(textItem(lyrics,"Hello moon"));QTRY_VERIFY(textItem(lyrics,"月亮你好"));
        auto *title=visual(window->contentItem(),"currentTitle");auto *search=visual(window->contentItem(),"librarySearch");
        auto *next=textItem(upcoming,"下一首");QVERIFY(title&&search&&next);
        for(const auto &region:regions){
            for(const auto &key:regions)flags[key]=key==region;
            QVERIFY(bridge->saveFont({},flags));const auto selected=bridge->fontFamilyFor(region);QVERIFY(!selected.isEmpty());
            const auto expected=[&](const char *key){return region==QLatin1String(key)?selected:QString("Microsoft YaHei UI");};
            QTRY_COMPARE(family(textItem(list,fixture.a.title)),expected("songs"));
            QTRY_COMPARE(family(textItem(lyrics,"Hello moon")),expected("lyrics"));
            QTRY_COMPARE(family(textItem(lyrics,"月亮你好")),expected("lyrics"));
            QTRY_COMPARE(family(title),expected("footer"));QTRY_COMPARE(family(next),expected("next"));QTRY_COMPARE(family(search),expected("rest"));
            record("font-region",{{"selectedRegion",region},{"selectedFamily",selected},{"song",family(textItem(list,fixture.a.title))},
                {"lyric",family(textItem(lyrics,"Hello moon"))},{"translation",family(textItem(lyrics,"月亮你好"))},
                {"footer",family(title)},{"next",family(next)},{"rest",family(search)}});
        }
        QVERIFY(window->grabWindow().save(evidence+"/font-regions.png"));QVERIFY(bridge->resetFont());
        QVERIFY2(fixture.shell->warnings().isEmpty(),qPrintable(fixture.shell->warnings().join('\n')));
    }
    void managedSharedBackgroundRendersAcrossPageTransitions(){
        Fixture fixture(2);QVERIFY(prepare(fixture));auto *window=fixture.shell->window();auto *bridge=fixture.shell->settingsBridge();
        QImage image(200,100,QImage::Format_RGB32);image.fill(QColor("#2244dd"));
        for(int y=0;y<100;++y)for(int x=0;x<100;++x)image.setPixelColor(x,y,QColor("#dd4422"));
        const auto original=fixture.dir.filePath("背景 image.png");QVERIFY(image.save(original));
        bridge->beginAppearance();QVERIFY(bridge->setAppearanceMode(4));QVERIFY(bridge->editAppearance("sourceLeft",QUrl::fromLocalFile(original)));
        QVERIFY(bridge->editAppearance("shareLR",true));QVERIFY(bridge->editAppearance("blurLeft",0));QVERIFY(bridge->saveAppearance());
        auto *paper=visual(window->contentItem(),"paperSurface");QVERIFY(paper);
        auto correct=[&]{
            const auto shot=window->grabWindow();if(shot.isNull())return false;const auto dpr=shot.devicePixelRatio();
            const auto area=paper->mapRectToScene(QRectF(QPointF(),paper->size()));
            const auto left=shot.pixelColor(qRound((area.x()+area.width()*.2)*dpr),qRound((area.y()+area.height()*.9)*dpr));
            const auto right=shot.pixelColor(qRound((area.x()+area.width()*.8)*dpr),qRound((area.y()+area.height()*.9)*dpr));
            return left.red()>170&&left.red()>left.blue()+70&&right.blue()>170&&right.blue()>right.red()+70;
        };
        QTRY_VERIFY(correct());
        auto *button=visual(window->contentItem(),"navigation1");QVERIFY(button);const auto point=button->mapToScene(QPointF(button->width()/2,button->height()/2));
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());QElapsedTimer timer;timer.start();int frames=0;
        while(timer.elapsed()<400){QVERIFY2(correct(),"Shared image background disappeared or turned black during a page transition");++frames;QTest::qWait(10);}
        QVERIFY(frames>5);QCOMPARE(fixture.shell->appBridge()->page(),1);QVERIFY(window->grabWindow().save(evidence+"/managed-background.png"));
        const auto state=fixture.library.setting("appearanceHistory").toObject();QVERIFY(!state.isEmpty());
        record("shared-managed-image",{{"frames",frames},{"actualImagePixels",true},{"transition",true},{"storedInExistingAppearanceHistory",true}});
        QVERIFY2(fixture.shell->warnings().isEmpty(),qPrintable(fixture.shell->warnings().join('\n')));
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);C6ParityTests tests;return QTest::qExec(&tests,argc,argv);}
#include "qml-c6-parity-tests.moc"
