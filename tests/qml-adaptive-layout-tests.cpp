#include "c4c5-ui-fixture.h"
#include "search-parameters-fixture.h"
#include "ui-bridge/ui-images.h"
#include "shortcut-rules.h"
#include <QApplication>
#include <QDir>
#include <QSignalSpy>
#include <QColorDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QMimeData>
#include <QScopeGuard>
#include <QWheelEvent>
#include <QScreen>
#include <QEnterEvent>
#include <QUrlQuery>
#include <QJSValue>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <thread>
#include <atomic>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif
using namespace c4c5_fixture;
namespace {
#ifdef Q_OS_WIN
// Observe every actual HWND change, including intermediate rectangles which
// Qt's logical-coordinate notifications and grabWindow cannot expose.
class NativeGeometryTrace final:public QAbstractNativeEventFilter {
public:
    explicit NativeGeometryTrace(QQuickWindow *window):handle(reinterpret_cast<HWND>(window->winId())){QCoreApplication::instance()->installNativeEventFilter(this);}
    ~NativeGeometryTrace(){QCoreApplication::instance()->removeNativeEventFilter(this);}
    QRect current()const{RECT r{};GetWindowRect(handle,&r);return {int(r.left),int(r.top),int(r.right-r.left),int(r.bottom-r.top)};}
    bool nativeEventFilter(const QByteArray &,void *message,qintptr *)override{
        auto *event=static_cast<MSG *>(message);
        if(event->hwnd==handle&&event->message==WM_WINDOWPOSCHANGED)rectangles.append(current());
        return false;
    }
    HWND handle;QList<QRect> rectangles;
};
bool nativeDrag(QQuickWindow *window,QPoint start,const QList<QPoint> &path){
    const auto handle=reinterpret_cast<HWND>(window->winId());
    SetForegroundWindow(handle);window->requestActivate();QTest::qWait(150);
    if(GetForegroundWindow()!=handle)return false;
    POINT saved{};GetCursorPos(&saved);std::atomic<bool> finished=false,interrupted=false;
    std::thread input([&]{
        const auto button=[](DWORD flag){INPUT event{};event.type=INPUT_MOUSE;event.mi.dwFlags=flag;return SendInput(1,&event,sizeof(event))==1;};
        SetCursorPos(start.x(),start.y());Sleep(70);
        if(GetForegroundWindow()!=handle){interrupted=true;finished=true;SetCursorPos(saved.x,saved.y);return;}
        const bool pressed=button(MOUSEEVENTF_LEFTDOWN);interrupted=!pressed;Sleep(80);
        auto from=start;
        for(const auto &to:path){
            if(interrupted)break;
            // Request input every 1 ms to expose updates between display frames.
            for(int i=1;i<=500;++i){
                if(GetForegroundWindow()!=handle){interrupted=true;break;}
                const QPoint p=from+(to-from)*i/500;SetCursorPos(p.x(),p.y());Sleep(1);
            }
            Sleep(500);from=to;
        }
        if(pressed)button(MOUSEEVENTF_LEFTUP);
        Sleep(250);SetCursorPos(saved.x,saved.y);finished=true;
    });
    const auto cleanup=qScopeGuard([&]{input.join();});
    return QTest::qWaitFor([&]{return finished.load();},10000)&&!interrupted;
}
#endif
QRectF rect(QQuickItem *item) { return item->mapRectToScene(QRectF(QPointF(),item->size())); }
bool click(QQuickWindow *w,QQuickItem *item) {
    if(!item||!item->isVisible()||!item->isEnabled())return false;
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,rect(item).center().toPoint());return true;
}
bool prepare(Fixture &f) {
    if(!f.ok)return false;
    auto *w=f.shell->window();w->showNormal();w->setGeometry(40,40,1240,790);
    if(!QTest::qWaitForWindowExposed(w))return false;
    return QTest::qWaitFor([&]{return f.shell->facade()->playing();});
}
void shot(QQuickWindow *w,const QString &name) {
    const auto folder=qEnvironmentVariable("RELAXER_ADAPTIVE_SCREENSHOTS");if(folder.isEmpty())return;
    QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(name+".png"));
}
bool miniSettled(QQuickWindow *w) {
    return QTest::qWaitFor([&]{w->grabWindow();return !w->property("miniSwitching").toBool();});
}
void wheel(QQuickWindow *w,QPoint point,int delta) {
    QWheelEvent e(point,w->mapToGlobal(point),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    // Match QtTest's mouse clock: repeated zero-timestamp wheel events are
    // coalesced/ignored by Qt Quick's gesture delivery on some render backends.
    e.setTimestamp(QTest::lastMouseTimestamp+=20);
    QCoreApplication::sendEvent(w,&e);
}
void miniHover(QQuickWindow *w,bool hover){
    // A hidden shell and newly loaded mini view must finish layout before input.
    w->grabWindow();QCoreApplication::processEvents();
    if(hover){
        const QPoint point(200,34);QEnterEvent enter(point,point,w->mapToGlobal(point));QCoreApplication::sendEvent(w,&enter);
        QTest::mouseMove(w,QPoint(199,34));QTest::mouseMove(w,point);
    }
    else{QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(w,&leave);}
    // Resizing the native window can defer a fade's first rendered frame.
    // Wait for the requested presentation rather than one wall-clock delay.
    QVERIFY(QTest::qWaitFor([&]{w->grabWindow();auto *mini=visual(w->contentItem(),"miniPlayer");return mini&&mini->property("controlsProgress").toReal()==(hover?1.:0.);},2000));
}
}
class AdaptiveLayoutTests final:public QObject {
    Q_OBJECT
private slots:
    void miniTransitionNaturalFrames_data(){
        QTest::addColumn<int>("width");
        QTest::newRow("wide")<<1240;QTest::newRow("narrow")<<460;
    }
    void miniTransitionNaturalFrames(){
        QFETCH(int,width);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        w->resize(width,790);QTest::qWait(600);
        // Do not call grabWindow here: it forces rendering and hid the native
        // resize/render-loop feedback in the original gesture regressions.
        struct Frames{QElapsedTimer clock;QMutex mutex;QList<qint64> times;QList<QPair<qint64,qreal>> ticks;};
        const auto frames=std::make_shared<Frames>();frames->clock.start();
        const auto connection=connect(w,&QQuickWindow::frameSwapped,w,[frames]{
            QMutexLocker lock(&frames->mutex);frames->times.append(frames->clock.nsecsElapsed());
        },Qt::DirectConnection);
        const auto animations=connect(w,&QQuickWindow::afterAnimating,w,[frames,w]{
            QMutexLocker lock(&frames->mutex);frames->ticks.append({frames->clock.nsecsElapsed(),w->property("miniTransitionProgress").toReal()});
        });
        const auto cleanup=qScopeGuard([&]{disconnect(connection);disconnect(animations);});
#ifdef Q_OS_WIN
        NativeGeometryTrace native(w);const auto nativeStart=native.current();
#endif
        for(const bool entering:{true,false}){
#ifdef Q_OS_WIN
            native.rectangles.clear();
#endif
            {QMutexLocker lock(&frames->mutex);frames->times.clear();frames->ticks.clear();}
            const auto start=frames->clock.nsecsElapsed();
            QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);
            QCOMPARE(w->property("miniMode").toBool(),entering);
            QVERIFY(QTest::qWaitFor([&]{return !w->property("miniSwitching").toBool();},4000));
            QTest::qWait(60);
            QList<qint64> samples;QList<QPair<qint64,qreal>> ticks;{QMutexLocker lock(&frames->mutex);samples=frames->times;ticks=frames->ticks;}
            qreal longest=0;auto previous=start,longestAt=start;
            for(auto frame:samples){if((frame-previous)/1e6>longest){longest=(frame-previous)/1e6;longestAt=frame;}previous=frame;}
            qreal gapProgress=entering?0:1;for(const auto &tick:ticks){if(tick.first>longestAt)break;gapProgress=tick.second;}
            qInfo().nospace()<<"natural-frames width="<<width<<" entering="<<entering
                <<" frames="<<samples.size()<<" firstMs="<<(samples.isEmpty()?0:(samples.first()-start)/1e6)
                <<" maxGapMs="<<longest<<" gapProgress="<<gapProgress<<" elapsedMs="<<(frames->clock.nsecsElapsed()-start)/1e6
                <<" dpr="<<w->devicePixelRatio()<<" graphicsApi="<<int(w->rendererInterface()->graphicsApi());
            QVERIFY(samples.size()>1);
#ifdef Q_OS_WIN
            int centerDrift=0,topDrift=0;
            for(const auto &r:native.rectangles){centerDrift=qMax(centerDrift,qAbs((2*r.x()+r.width())-(2*nativeStart.x()+nativeStart.width())));topDrift=qMax(topDrift,qAbs(r.y()-nativeStart.y()));}
            qInfo()<<"shortcut native horizontal center drift (half pixels)"<<centerDrift<<"top drift"<<topDrift;
            QVERIFY(centerDrift<=1);QCOMPARE(topDrift,0);
#endif
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniElasticReturnKeepsFixedCorner_data(){
        expandedCornerStaysUnderThePointerAndStationaryInputDoesNotJitter_data();
    }
    void shortcutFlightDoesNotHandNewPressesToSystemResize(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        auto *handle=visual(w->contentItem(),QString("windowResizeEdge-%1").arg(int(Qt::TopEdge|Qt::RightEdge)));QVERIFY(handle);
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));
        auto *motion=w->findChild<QObject *>("miniGeometryMotion");QVERIFY(motion);QVERIFY(QMetaObject::invokeMethod(motion,"pause"));
        // MouseArea shadows Item.enabled; inspect its QML property rather than
        // the C++ QQuickItem::isEnabled() getter for the base class.
        QVERIFY(w->property("miniSwitching").toBool());QVERIFY(!handle->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(motion,"resume"));QVERIFY(miniSettled(w));QVERIFY(handle->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(w,"leaveMiniPlayer"));QVERIFY(QMetaObject::invokeMethod(motion,"pause"));
        QVERIFY(!handle->property("enabled").toBool());QVERIFY(QMetaObject::invokeMethod(motion,"resume"));QVERIFY(miniSettled(w));QVERIFY(handle->property("enabled").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniElasticReturnKeepsFixedCorner(){
        QFETCH(int,edge);QFETCH(QPoint,travel);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));
        auto *handle=visual(w->contentItem(),QString("miniResizeEdge-%1").arg(edge));QVERIFY(handle);
        const auto start=rect(handle).center().toPoint(),global=w->mapToGlobal(start);
        const auto opposite=[&]{return QPoint(edge&Qt::LeftEdge?w->x()+w->width():w->x(),edge&Qt::TopEdge?w->y()+w->height():w->y());};
        const auto fixed=opposite();
#ifdef Q_OS_WIN
        NativeGeometryTrace native(w);const auto nativeStart=native.current();
        const QPoint nativeFixed(edge&Qt::LeftEdge?nativeStart.x()+nativeStart.width():nativeStart.x(),edge&Qt::TopEdge?nativeStart.y()+nativeStart.height():nativeStart.y());
#endif
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);
        const auto pointer=global+travel/2;QTest::mouseMove(w,w->mapFromGlobal(pointer),20);
        QVERIFY(w->property("miniMode").toBool());QVERIFY(w->width()>340);QVERIFY(w->height()>58);
        int deviation=0,updates=0;
        const auto sample=[&]{++updates;deviation=qMax(deviation,(opposite()-fixed).manhattanLength());};
        const auto x=connect(w,&QWindow::xChanged,w,sample),y=connect(w,&QWindow::yChanged,w,sample);
        const auto width=connect(w,&QWindow::widthChanged,w,sample),height=connect(w,&QWindow::heightChanged,w,sample);
        const auto cleanup=qScopeGuard([&]{disconnect(x);disconnect(y);disconnect(width);disconnect(height);});
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(pointer));
        QVERIFY(QTest::qWaitFor([&]{return w->size()==QSize(340,58);},2000));
        QVERIFY(updates>0);
        qInfo()<<"elastic return maximum intermediate anchor drift"<<deviation;
#ifdef Q_OS_WIN
        int nativeDeviation=0;
        for(const auto &r:native.rectangles){const QPoint p(edge&Qt::LeftEdge?r.x()+r.width():r.x(),edge&Qt::TopEdge?r.y()+r.height():r.y());nativeDeviation=qMax(nativeDeviation,(p-nativeFixed).manhattanLength());}
        qInfo()<<"native elastic anchor drift"<<nativeDeviation<<"native updates"<<native.rectangles.size();
        QCOMPARE(nativeDeviation,0);
        // The HWND is authoritative. At fractional DPI an unrepresentable
        // native origin and size can round to a logical sum one point away.
        QVERIFY((opposite()-fixed).manhattanLength()<=2);QVERIFY(deviation<=2);
#else
        QCOMPARE(opposite(),fixed);QCOMPARE(deviation,0);
#endif
    }
#ifdef Q_OS_WIN
    void nativeCornerGesture_data(){
        QTest::addColumn<int>("edge");QTest::addColumn<bool>("mini");QTest::addColumn<int>("width");
        for(int width:{460,900,340})for(int edge:{int(Qt::TopEdge|Qt::LeftEdge),int(Qt::TopEdge|Qt::RightEdge),int(Qt::BottomEdge|Qt::LeftEdge),int(Qt::BottomEdge|Qt::RightEdge)})
            QTest::newRow(qPrintable(QString("%1-%2").arg(width).arg(edge)))<<edge<<(width==340)<<width;
    }
    void nativeCornerGesture(){
        if(!qEnvironmentVariableIsSet("RELAXER_NATIVE_RESIZE"))QSKIP("Opt-in desktop input probe: RELAXER_NATIVE_RESIZE=1");
        QFETCH(int,edge);QFETCH(bool,mini);QFETCH(int,width);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        w->setGeometry(360,120,mini?600:width,600);QTest::qWait(400);
        if(mini){QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(QTest::qWaitFor([&]{return !w->property("miniSwitching").toBool();}));
            w->setPosition(edge&Qt::LeftEdge?850:350,edge&Qt::TopEdge?700:140);QTest::qWait(200);}
        auto *handle=visual(w->contentItem(),QString(mini?"miniResizeEdge-%1":"windowResizeEdge-%1").arg(edge));QVERIFY(handle);
        NativeGeometryTrace native(w);const auto before=native.current();
        const auto local=rect(handle).center();POINT point{LONG(qRound(local.x()*w->devicePixelRatio())),LONG(qRound(local.y()*w->devicePixelRatio()))};ClientToScreen(native.handle,&point);
        const QPoint start(point.x,point.y),fixed(edge&Qt::LeftEdge?before.x()+before.width():before.x(),edge&Qt::TopEdge?before.y()+before.height():before.y());
        const int sx=edge&Qt::LeftEdge?-1:1,sy=edge&Qt::TopEdge?-1:1;
        const auto ratio=w->devicePixelRatio();
        const QPoint end=start+QPoint(qRound(sx*160*ratio),qRound(sy*(mini?520:40)*ratio));
        const auto key=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        int changes=0,stationaryChanges=0;bool lastMini=mini;QPoint previousPointer;QRect previousBounds;QElapsedTimer stationary;stationary.start();
        QTimer observer;observer.setInterval(10);
        connect(&observer,&QTimer::timeout,w,[&]{
            const bool mode=w->property("miniMode").toBool();if(mode!=lastMini){++changes;lastMini=mode;}
            POINT p{};GetCursorPos(&p);const QPoint pointer(p.x,p.y);const auto bounds=native.current();
            if(pointer!=previousPointer||w->property("miniSwitching").toBool())stationary.restart();
            else if(stationary.elapsed()>100&&previousBounds!=bounds)++stationaryChanges;
            previousPointer=pointer;previousBounds=bounds;
        });observer.start();
        QList<QPoint> path{end};
        if(mini)path<<start+QPoint(qRound(sx*30*ratio),qRound(sy*30*ratio))<<end;
        QVERIFY(nativeDrag(w,start,path));observer.stop();
        QVERIFY(!w->property("windowResizeActive").toBool());
        int drift=0;
        for(const auto &r:native.rectangles){const QPoint p(edge&Qt::LeftEdge?r.x()+r.width():r.x(),edge&Qt::TopEdge?r.y()+r.height():r.y());drift=qMax(drift,(p-fixed).manhattanLength());}
        qInfo()<<"native gesture width"<<width<<"edge"<<edge<<"updates"<<native.rectangles.size()<<"anchorDriftPx"<<drift<<"stationaryChanges"<<stationaryChanges<<"modeChanges"<<changes<<"final"<<native.current();
        QVERIFY(!w->property("miniMode").toBool());
        QCOMPARE(drift,0);
        QCOMPARE(stationaryChanges,0);QCOMPARE(changes,mini?3:0);
        QCOMPARE(f.shell->facade()->current().id,key);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void nativeMoveAndSystemEdges_data(){
        QTest::addColumn<bool>("mini");QTest::addColumn<int>("edge");
        QTest::newRow("normal-move")<<false<<0;QTest::newRow("mini-move")<<true<<0;
        for(int edge:{int(Qt::LeftEdge),int(Qt::RightEdge),int(Qt::TopEdge),int(Qt::BottomEdge)})QTest::newRow(qPrintable(QString("system-edge-%1").arg(edge)))<<false<<edge;
    }
    void nativeMoveAndSystemEdges(){
        if(!qEnvironmentVariableIsSet("RELAXER_NATIVE_RESIZE"))QSKIP("Opt-in desktop input probe: RELAXER_NATIVE_RESIZE=1");
        QFETCH(bool,mini);QFETCH(int,edge);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();w->setGeometry(360,120,900,600);QTest::qWait(400);
        if(mini){QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));}
        auto *handle=visual(w->contentItem(),edge?QString("windowResizeEdge-%1").arg(edge):mini?"miniCoverDrag":"windowDragArea");QVERIFY(handle);
        NativeGeometryTrace native(w);const auto before=native.current(),logical=w->geometry();const auto ratio=w->devicePixelRatio();
        const auto local=rect(handle).center();POINT point{LONG(qRound(local.x()*ratio)),LONG(qRound(local.y()*ratio))};ClientToScreen(native.handle,&point);
        const QPoint start(point.x,point.y),end=start+QPoint(qRound(90*ratio),qRound(60*ratio));
        bool customStarted=false;QTimer observer;observer.setInterval(10);connect(&observer,&QTimer::timeout,w,[&]{customStarted|=w->property("windowResizeActive").toBool();});observer.start();
        QVERIFY(nativeDrag(w,start,{end}));observer.stop();QVERIFY(!customStarted);QCOMPARE(w->property("miniMode").toBool(),mini);
        if(!edge){QCOMPARE(w->size(),logical.size());QVERIFY((w->position()-logical.topLeft()-QPoint(90,60)).manhattanLength()<=1);}
        else{
            const QSize expected(logical.width()+(edge==Qt::LeftEdge?-90:edge==Qt::RightEdge?90:0),logical.height()+(edge==Qt::TopEdge?-60:edge==Qt::BottomEdge?60:0));
            QCOMPARE(w->size(),expected);
            for(const auto &r:native.rectangles){
                if(edge==Qt::LeftEdge)QCOMPARE(r.x()+r.width(),before.x()+before.width());
                else if(edge==Qt::RightEdge)QCOMPARE(r.x(),before.x());
                else if(edge==Qt::TopEdge)QCOMPARE(r.y()+r.height(),before.y()+before.height());
                else QCOMPARE(r.y(),before.y());
            }
        }
        QVERIFY(f.shell->facade()->playing());QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniGlobalShortcutRegistersWithWindowsAndWorksWhileHidden(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *integration=f.shell->findChild<UiIntegration *>();QVERIFY(integration);
        auto config=ShortcutRules::defaults();auto globals=config["global"].toObject();
        for(const auto &key:ShortcutRules::actionKeys())globals[key]="";
        globals["miniPlayer"]="Ctrl+Alt+Shift+F11";config["global"]=globals;config["globalEnabled"]=true;
        QVERIFY2(integration->saveShortcuts(config.toVariantMap()),qPrintable(integration->shortcutStatus()));
        const auto binding=ShortcutRules::nativeKey(QKeySequence("Ctrl+Alt+Shift+F11"));
        const bool duplicate=RegisterHotKey(nullptr,0x7f01,binding.modifiers,binding.key);
        if(duplicate)UnregisterHotKey(nullptr,0x7f01);QVERIFY(!duplicate);
        const auto track=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        w->hide();QTest::qWait(350);QSignalSpy toggles(integration,&UiIntegration::miniPlayerToggleRequested);
        MSG message{};message.message=WM_HOTKEY;qintptr result=0;bool dispatched=false;
        // Only one global binding is registered in this fixture. Exercise the
        // Windows message path without sending keyboard input to other apps.
        for(int id=0x5300;id<0x7f00&&!dispatched;++id){message.wParam=id;dispatched=integration->nativeEventFilter("windows_dispatcher_MSG",&message,&result);}
        QVERIFY(dispatched);QCOMPARE(toggles.count(),1);QVERIFY(miniSettled(w));QVERIFY(w->property("miniMode").toBool());QVERIFY(w->isVisible());
        QVERIFY(integration->nativeEventFilter("windows_dispatcher_MSG",&message,&result));QVERIFY(miniSettled(w));QVERIFY(!w->property("miniMode").toBool());
        config["globalEnabled"]=false;QVERIFY(integration->saveShortcuts(config.toVariantMap()));
        QVERIFY(RegisterHotKey(nullptr,0x7f01,binding.modifiers,binding.key));UnregisterHotKey(nullptr,0x7f01);
        QCOMPARE(f.shell->facade()->current().id,track);QCOMPARE(f.shell->facade()->queue().items,queue);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
#endif
    void narrowingLyricsWorkspaceCancelsAllPendingWorkAndReturnsToLocalMusic_data(){
        QTest::addColumn<bool>("reduceMotion");
        QTest::newRow("animated")<<false;QTest::newRow("reduced-motion")<<true;
    }
    void narrowingLyricsWorkspaceCancelsAllPendingWorkAndReturnsToLocalMusic(){
        QFETCH(bool,reduceMotion);
        search_fixture::Network network;network.respond=nullptr;
        Fixture f;QVERIFY(f.ok);f.shell.reset();
        f.shell=std::make_unique<QmlShell>(&f.library,nullptr,true,f.runtime.get(),nullptr,SettingsBridge::SystemThemeReader{},&network);
        QVERIFY(f.shell->load());QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",reduceMotion));
        f.shell->facade()->play({f.a.id,f.b.id},f.a.id);QVERIFY(prepare(f));auto *w=f.shell->window();
        auto *search=f.shell->lyricsSearchBridge();auto *translation=f.shell->translationBridge();f.http.setHold(true);
        QList<quint64> tasks;
        for(auto id:{f.a.id,f.b.id}){QVERIFY(translation->setTargetTrack(QString::number(id)));QVERIFY(translation->classify(1));const auto token=translation->prepareAi();QVERIFY(!token.isEmpty());tasks.append(token.toULongLong());QVERIFY(translation->start(token));}
        QTRY_VERIFY(f.http.heldCount()>0);
        const QVariant keys=QStringList{QString::number(f.a.id),QString::number(f.b.id)};
        QVERIFY(QMetaObject::invokeMethod(w,"openLyricsTools",Q_ARG(QVariant,keys),Q_ARG(QVariant,QVariant(false))));QTRY_VERIFY(w->property("workspaceVisible").toBool());
        search->setSearchOptions(true,true);QVERIFY(search->searchTargets(true));QTRY_VERIFY(!network.replies.isEmpty());QVERIFY(search->busy());
        const auto queue=f.shell->facade()->queue().items;const auto track=f.shell->facade()->current().id;
        w->resize(547,790);w->grabWindow();QVERIFY(w->property("workspaceOpened").toBool());QVERIFY(search->busy());QVERIFY(translation->unfinished());
        w->resize(546,790);QTRY_VERIFY(!w->property("workspaceVisible").toBool());QVERIFY(!search->busy());QVERIFY(!translation->unfinished());
        for(auto task:tasks)QCOMPARE(f.shell->translationCoordinator()->summary(task).state,TranslationState::Cancelled);
        QCOMPARE(f.shell->appBridge()->page(),0);QCOMPARE(f.shell->libraryBridge()->playlistId(),QString("0"));
        for(const auto &reply:network.replies)if(reply)QVERIFY(reply->isFinished());
        QCOMPARE(LyricsDocumentReader::readFile(f.library.absolute(f.library.track(f.a.id).lyrics)).bytes,f.source);
        QCOMPARE(f.shell->facade()->current().id,track);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->facade()->playing());
        w->resize(1240,790);QVERIFY(translation->setTargetTrack(QString::number(f.a.id)));const auto again=translation->prepareAi();QVERIFY(!again.isEmpty());QVERIFY(translation->start(again));translation->cancelAll();
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void backgroundClosesPopupWindowsAndReleasesTheirEffects(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));
        auto *mini=visual(w->contentItem(),"miniPlayer");QVERIFY(mini);QVERIFY(QMetaObject::invokeMethod(mini,"openQueue"));auto *queue=mini->findChild<QObject *>("miniQueuePopup");QVERIFY(queue);QTRY_VERIFY(queue->property("opened").toBool());
        auto *surface=queue->property("contentItem").value<QQuickItem *>();QVERIFY(surface);auto *floating=surface->window();QVERIFY(floating!=w);floating->grabWindow();
        QVERIFY(!floating->isPersistentGraphics());QVERIFY(!floating->isPersistentSceneGraph());
        auto *glass=visual(surface,"miniQueueGlass");QVERIFY(glass);QVERIFY(glass->property("presentationActive").toBool());
        const auto position=f.shell->facade()->position();w->showMinimized();QTRY_VERIFY(!queue->property("visible").toBool());QVERIFY(!glass->property("presentationActive").toBool());QVERIFY(!visual(glass,"glassBackdropBlur"));
        QTest::qWait(500);QVERIFY(f.shell->facade()->position()>position);w->showNormal();QVERIFY(QTest::qWaitForWindowExposed(w));
        auto *volume=visual(mini,"miniVolume");QVERIFY(QMetaObject::invokeMethod(volume,"openVolume"));auto *popup=volume->findChild<QObject *>("compactVolumePopup");QVERIFY(popup);QTRY_VERIFY(popup->property("opened").toBool());
        floating=popup->property("contentItem").value<QQuickItem *>()->window();QVERIFY(!floating->isPersistentGraphics());QVERIFY(!floating->isPersistentSceneGraph());
        w->showMinimized();QTRY_VERIFY(!popup->property("visible").toBool());w->showNormal();QVERIFY(QTest::qWaitForWindowExposed(w));QVERIFY(QMetaObject::invokeMethod(mini,"openQueue"));QTRY_VERIFY(queue->property("opened").toBool());
        QVERIFY(glass->property("presentationActive").toBool());QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void expandedCornerStaysUnderThePointerAndStationaryInputDoesNotJitter_data(){
        QTest::addColumn<int>("edge");QTest::addColumn<QPoint>("travel");
        QTest::newRow("top-left")<<int(Qt::TopEdge|Qt::LeftEdge)<<QPoint(-240,-482);
        QTest::newRow("top-right")<<int(Qt::TopEdge|Qt::RightEdge)<<QPoint(240,-482);
        QTest::newRow("bottom-left")<<int(Qt::BottomEdge|Qt::LeftEdge)<<QPoint(-240,482);
        QTest::newRow("bottom-right")<<int(Qt::BottomEdge|Qt::RightEdge)<<QPoint(240,482);
    }
    void expandedCornerStaysUnderThePointerAndStationaryInputDoesNotJitter(){
        QFETCH(int,edge);QFETCH(QPoint,travel);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));
        auto *handle=visual(root,QString("miniResizeEdge-%1").arg(edge));QVERIFY(handle);const auto start=rect(handle).center().toPoint();
        const auto global=w->mapToGlobal(start);const QPoint inset=global-QPoint(edge&Qt::RightEdge?w->x()+w->width():w->x(),edge&Qt::BottomEdge?w->y()+w->height():w->y());
        const QPoint opposite(edge&Qt::LeftEdge?w->x()+w->width():w->x(),edge&Qt::TopEdge?w->y()+w->height():w->y());
        const auto pointer=global+travel;
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(w,w->mapFromGlobal(global+travel*3/4),20);QVERIFY(w->property("miniMode").toBool());
        QTest::mouseMove(w,w->mapFromGlobal(pointer),20);
        QVERIFY(QTest::qWaitFor([&]{w->grabWindow();const QPoint current(edge&Qt::LeftEdge?w->x()+w->width():w->x(),edge&Qt::TopEdge?w->y()+w->height():w->y());if((current-opposite).manhattanLength()>2)return true;return !w->property("miniSwitching").toBool();}));
        QVERIFY(!w->property("miniSwitching").toBool());QVERIFY(!w->property("miniMode").toBool());
        QCOMPARE(QPoint(edge&Qt::LeftEdge?w->x()+w->width():w->x(),edge&Qt::TopEdge?w->y()+w->height():w->y()),opposite);
        const QPoint corner(edge&Qt::RightEdge?w->x()+w->width():w->x(),edge&Qt::BottomEdge?w->y()+w->height():w->y());QCOMPARE(corner+inset,pointer);
        auto *integration=f.shell->findChild<UiIntegration *>();QVERIFY(integration);QCOMPARE(integration->pointerPosition(),QPointF(pointer));
        const auto geometry=w->geometry();QSignalSpy widths(w,&QWindow::widthChanged),heights(w,&QWindow::heightChanged),xs(w,&QWindow::xChanged),ys(w,&QWindow::yChanged);
        for(int i=0;i<8;++i){QTest::mouseMove(w,w->mapFromGlobal(pointer),10);w->grabWindow();QCoreApplication::processEvents();QCOMPARE(w->geometry(),geometry);}
        QCOMPARE(widths.count()+heights.count()+xs.count()+ys.count(),0);QVERIFY(handle->property("pressed").toBool());
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(pointer));QVERIFY(!w->property("windowResizeActive").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void expandedPageAndHiddenArtworkAreReleasedWithoutLosingPlayback(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();QVERIFY(!visual(root,"nowPlayingPage"));
        auto *cover=visual(root,"bottomCover");QVERIFY(cover);QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return !cover->property("displayedUrl").toUrl().isEmpty();}));
        const auto key=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return w->property("playerProgress").toReal()==1.;}));QVERIFY(visual(root,"nowPlayingPage"));
        const auto target=w->property("coverTo").value<QJSValue>().toVariant().toMap();
        const auto landed=rect(visual(root,"currentArtwork"));
        QVERIFY(qAbs(target["x"].toReal()-landed.x())<1);QVERIFY(qAbs(target["y"].toReal()-landed.y())<1);
        QVERIFY(qAbs(target["width"].toReal()-landed.width())<1);QVERIFY(qAbs(target["height"].toReal()-landed.height())<1);
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return w->property("playerProgress").toReal()==0.;}));
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));QCOMPARE(cover->property("displayedUrl").toUrl(),QUrl());
        QVERIFY(visual(root,"miniCover")->property("presentationActive").toBool());
        w->showMinimized();QTest::qWait(450);QVERIFY(!visual(root,"miniCover")->property("presentationActive").toBool());
        w->showNormal();QVERIFY(miniSettled(w));QVERIFY(QMetaObject::invokeMethod(w,"leaveMiniPlayer"));QVERIFY(miniSettled(w));
        QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return !cover->property("displayedUrl").toUrl().isEmpty();}));
        QCOMPARE(f.shell->facade()->current().id,key);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniGestureKeepsTheSameGrabAcrossRepeatedModeChanges_data(){
        QTest::addColumn<int>("edge");QTest::addColumn<int>("width");QTest::addColumn<QPoint>("inward");QTest::addColumn<QPoint>("outward");
        QTest::newRow("top-narrow")<<int(Qt::TopEdge)<<460<<QPoint(0,80)<<QPoint(0,-80);
        QTest::newRow("bottom-narrow")<<int(Qt::BottomEdge)<<460<<QPoint(0,-80)<<QPoint(0,80);
        QTest::newRow("wide-corner")<<int(Qt::BottomEdge|Qt::RightEdge)<<900<<QPoint(-100,-80)<<QPoint(240,240);
    }
    void miniGestureKeepsTheSameGrabAcrossRepeatedModeChanges(){
        QFETCH(int,edge);QFETCH(int,width);QFETCH(QPoint,inward);QFETCH(QPoint,outward);
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();w->resize(width,540);w->grabWindow();QTest::qWait(350);w->grabWindow();
        const auto key=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        auto *handle=visual(root,QString("windowResizeEdge-%1").arg(edge));QVERIFY(handle);
        const auto start=rect(handle).center().toPoint(),global=w->mapToGlobal(start);auto pointer=global;
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);
        for(int cycle=0;cycle<2;++cycle){
            pointer=global+inward;QTest::mouseMove(w,w->mapFromGlobal(pointer),20);
            QVERIFY(w->property("miniMode").toBool());QVERIFY(miniSettled(w));QVERIFY(handle->property("pressed").toBool());QVERIFY(w->property("windowResizeActive").toBool());
            pointer=global+inward+outward;QTest::mouseMove(w,w->mapFromGlobal(pointer),20);
            QVERIFY(!w->property("miniMode").toBool());QVERIFY(miniSettled(w));QVERIFY(handle->property("pressed").toBool());
            const auto before=w->size();
            const QPoint further=outward/4;QTest::mouseMove(w,w->mapFromGlobal(pointer+further),20);
            QVERIFY(w->height()>before.height());
            QTest::mouseMove(w,w->mapFromGlobal(pointer),20);
        }
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(pointer));QVERIFY(!w->property("windowResizeActive").toBool());QVERIFY(!w->flags().testFlag(Qt::WindowStaysOnTopHint));
        QCOMPARE(f.shell->facade()->current().id,key);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void keyboardMiniPositionIsRememberedOnlyForKeyboardRoundTrips(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *settings=f.shell->settingsBridge();
        const auto normal=w->geometry();QVERIFY(settings->miniWindowPosition().isEmpty());
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));
        const auto screen=w->screen()->availableGeometry();const QPoint saved=screen.topLeft()+QPoint(90,360);w->setPosition(saved);
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));QCOMPARE(w->geometry(),normal);
        QCOMPARE(settings->miniWindowPosition().value("x").toInt(),saved.x());QCOMPARE(settings->miniWindowPosition().value("y").toInt(),saved.y());
        {SettingsBridge reloaded(&f.library);QCOMPARE(reloaded.miniWindowPosition(),settings->miniWindowPosition());}
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));QCOMPARE(w->position(),saved);
        w->setPosition(saved+QPoint(30,30));QVERIFY(QMetaObject::invokeMethod(w,"leaveMiniPlayer"));QVERIFY(miniSettled(w));
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));QCOMPARE(w->position(),saved);
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));w->setPosition(saved+QPoint(60,60));
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));
        QCOMPARE(settings->miniWindowPosition().value("x").toInt(),saved.x());QCOMPARE(settings->miniWindowPosition().value("y").toInt(),saved.y());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniHandoffRemainsVisibleUntilItLands_data(){
        QTest::addColumn<int>("width");QTest::addColumn<bool>("controls");
        QTest::newRow("wide-text")<<1240<<false;QTest::newRow("narrow-text")<<460<<false;
        QTest::newRow("wide-buttons")<<1240<<true;QTest::newRow("narrow-buttons")<<460<<true;
    }
    void miniHandoffRemainsVisibleUntilItLands(){
        QFETCH(int,width);QFETCH(bool,controls);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();w->resize(width,790);w->grabWindow();QTest::qWait(350);w->grabWindow();
        if(!controls){f.shell->playbackBridge()->pause();QTRY_VERIFY(!f.shell->playbackBridge()->playing());}
        const auto normal=w->geometry();QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));miniHover(w,controls);
        QVERIFY(QMetaObject::invokeMethod(w,"leaveMiniPlayer"));auto *motion=w->findChild<QObject *>("miniGeometryMotion");QVERIFY(motion);QVERIFY(QMetaObject::invokeMethod(motion,"pause"));
        const QString key=controls?"play":"title";auto *flight=visual(root,"miniHandoff-"+key);QVERIFY(flight);
        for(const qreal progress:{.15,.55,.95,.999}){
            w->setProperty("miniTransitionProgress",1-progress);w->grabWindow();QCoreApplication::processEvents();
            QVERIFY(flight->isVisible());QCOMPARE(flight->opacity(),1.);
            QVERIFY(rect(flight).left()>=-1&&rect(flight).right()<=w->width()+1);QVERIFY(rect(flight).top()>=-1&&rect(flight).bottom()<=w->height()+1);
            if(progress==.95)shot(w,QString("mini-handoff-%1-%2").arg(width).arg(controls));
        }
        auto *target=visual(root,controls?"playPauseButton":"currentTitle");QVERIFY(target);
        const auto actual=flight->mapToGlobal(QPointF()),expected=normal.topLeft()+rect(target).topLeft();QVERIFY2(QLineF(actual,expected).length()<3,qPrintable(QString("Landing (%1,%2), target (%3,%4)").arg(actual.x()).arg(actual.y()).arg(expected.x()).arg(expected.y())));
        QVERIFY(QMetaObject::invokeMethod(motion,"complete"));QVERIFY(miniSettled(w));QCOMPARE(w->geometry(),normal);QVERIFY(!visual(root,"miniHandoff-"+key));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniLyricsUseOneOrTwoRowsWithoutMovingTheCover_data(){
        QTest::addColumn<bool>("translated");QTest::newRow("original-only")<<false;QTest::newRow("bilingual")<<true;
    }
    void miniLyricsUseOneOrTwoRowsWithoutMovingTheCover(){
        QFETCH(bool,translated);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *bridge=f.shell->playbackBridge();auto *lyrics=f.shell->lyricsModel();
        const QString original="夜空中最亮的星",translation="The brightest star in the sky";
        const auto source=("[00:00.00]"+original+"\n"+(translated?"[00:00.00]"+translation+"\n":QString())+"[00:30.00]下一句\n").toUtf8();
        QVERIFY(f.library.saveLyrics(f.a.id,source));QTRY_COMPARE(lyrics->textAt(0),original);
        QCOMPARE(lyrics->translationAt(-1),QString());QCOMPARE(lyrics->translationAt(999),QString());
        QCOMPARE(lyrics->translationAt(0),translated?translation:QString());
        const auto key=bridge->trackId();const auto queue=f.shell->facade()->queue().items;const auto native=w->winId();
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));miniHover(w,false);
        auto *mini=visual(w->contentItem(),"miniPlayer"),*cover=visual(mini,"miniCover"),*line=visual(mini,"miniLyric"),*sub=visual(mini,"miniTranslationViewport");QVERIFY(mini);QVERIFY(cover);QVERIFY(line);QVERIFY(sub);
        QTRY_VERIFY(mini->property("showingLyrics").toBool());QCOMPARE(line->property("text").toString(),original);QCOMPARE(sub->isVisible(),translated);
        QVERIFY(!visual(mini,"miniMetadata")->isVisible());QVERIFY(cover->isVisible());const auto coverBounds=rect(cover);
        auto *column=visual(mini,"miniLyrics");QVERIFY(column);QVERIFY(column->height()<=42);QVERIFY(qAbs(rect(column).center().y()-coverBounds.center().y())<1);
        if(translated){auto *text=visual(mini,"miniLyricTranslation");QCOMPARE(text->property("text").toString(),translation);QVERIFY(rect(line).bottom()+2<=rect(text).top());}
        QVERIFY(!mini->property("lyricOverflow").toBool());QVERIFY(!mini->property("marqueeActive").toBool());
        shot(w,translated?"mini-lyrics-bilingual":"mini-lyrics-single");
        bridge->pause();QTRY_VERIFY(!mini->property("showingLyrics").toBool());QVERIFY(visual(mini,"miniMetadata")->isVisible());QVERIFY(cover->isVisible());QCOMPARE(rect(cover),coverBounds);shot(w,"mini-lyrics-paused");
        bridge->play();QTRY_VERIFY(mini->property("showingLyrics").toBool());QCOMPARE(rect(cover),coverBounds);QCOMPARE(w->size(),QSize(340,58));QCOMPARE(w->winId(),native);
        QCOMPARE(bridge->trackId(),key);QCOMPARE(f.shell->facade()->queue().items,queue);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniLongLyricsReachBothEndsBeforeTheNextLineAndResync(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *bridge=f.shell->playbackBridge();auto *lyrics=f.shell->lyricsModel();
        const QString original="Follow the moon across the quiet sky and let every word of this long lyric reach the end of the little player";
        const QString translation="跟随月光越过安静的夜空，让这一句很长很长的歌词在小小的播放器中完整地慢慢展示出来";
        const auto source=("[00:00.00]"+original+"\n[00:00.00]"+translation+"\n[00:07.50]短句，无需翻译\n[00:20.00]"+original+"\n").toUtf8();
        QVERIFY(f.library.saveLyrics(f.a.id,source));QTRY_COMPARE(lyrics->textAt(0),original);QTRY_VERIFY(bridge->seekable());
        QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));miniHover(w,false);auto *mini=visual(w->contentItem(),"miniPlayer");QVERIFY(mini);
        auto *line=visual(mini,"miniLyric"),*sub=visual(mini,"miniLyricTranslation"),*viewport=visual(mini,"miniLyricViewport"),*subViewport=visual(mini,"miniTranslationViewport");QVERIFY(line);QVERIFY(sub);QVERIFY(viewport);QVERIFY(subViewport);
        auto *motion=mini->findChild<QObject *>("miniLyricScrollMotion");QVERIFY(motion);
        QTRY_VERIFY(mini->property("marqueeActive").toBool());QVERIFY(viewport->clip());QVERIFY(subViewport->clip());
        QVERIFY(QTest::qWaitFor([&]{return line->x()<-1&&sub->x()<-1;},1500));
        const auto epoch=mini->property("lyricScrollStart").toReal();QSignalSpy documents(lyrics,&LyricsModel::documentChanged);
        QVERIFY(f.library.favorite(f.a.id,true));QTRY_VERIFY(!documents.isEmpty());QCOMPARE(mini->property("lyricScrollStart").toReal(),epoch);
        bridge->seek(6000);QTRY_VERIFY(mini->property("lyricScrollStart").toReal()>=6000);
        QVERIFY(QTest::qWaitFor([&]{return mini->property("lyricScrollProgress").toReal()>=.999;},2200));
        QCOMPARE(lyrics->currentIndex(),0);QVERIFY(line->x()<0);QVERIFY(sub->x()<0);
        QVERIFY(qAbs(line->x()+line->width()-viewport->width())<1);QVERIFY(qAbs(sub->x()+sub->width()-subViewport->width())<1);shot(w,"mini-lyrics-scroll-end");
        QTRY_COMPARE(lyrics->currentIndex(),1);QTRY_COMPARE(line->property("text").toString(),QString("短句，无需翻译"));QVERIFY(!subViewport->isVisible());QVERIFY(line->x()>=0);QVERIFY(!motion->property("running").toBool());
        bridge->seek(4000);QTRY_COMPARE(lyrics->currentIndex(),0);QTRY_VERIFY(mini->property("lyricScrollStart").toReal()>=4000);
        QVERIFY(lyrics->setOffsetMs(1000));QTRY_COMPARE(mini->property("lyricEnd").toReal(),8500.);
        bridge->seek(55000);QTRY_COMPARE(lyrics->currentIndex(),2);QCOMPARE(mini->property("lyricEnd").toReal(),qreal(bridge->duration()));QTRY_VERIFY(motion->property("running").toBool());
        w->showMinimized();QTRY_VERIFY(!motion->property("running").toBool());QTest::qWait(150);QVERIFY(bridge->playing());
        w->showNormal();QVERIFY(QTest::qWaitForWindowExposed(w));miniHover(w,false);QTRY_VERIFY(motion->property("running").toBool());QVERIFY(mini->property("lyricScrollStart").toReal()>=55000);
        QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",true));QTRY_VERIFY(!motion->property("running").toBool());
        const auto reducedProgress=mini->property("lyricScrollProgress").toReal();QVERIFY(QTest::qWaitFor([&]{return mini->property("lyricScrollProgress").toReal()>reducedProgress;},800));
        QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",false));QTRY_VERIFY(motion->property("running").toBool());
        QVERIFY(f.library.saveLyrics(f.a.id,QString("纯音乐，请欣赏").toUtf8()));QTRY_VERIFY(!lyrics->timed());QTRY_VERIFY(!mini->property("showingLyrics").toBool());QVERIFY(visual(mini,"miniMetadata")->isVisible());QVERIFY(!motion->property("running").toBool());QVERIFY(visual(mini,"miniCover")->isVisible());
        bridge->next();QTRY_COMPARE(bridge->trackId(),QString::number(f.b.id));QTRY_VERIFY(mini->property("showingLyrics").toBool());
        QCOMPARE(line->property("text").toString(),lyrics->textAt(lyrics->currentIndex()));QVERIFY(!subViewport->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniShortcutCanBeEditedInTheSettingsTable(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();QVERIFY(f.shell->appBridge()->navigate(3));w->grabWindow();
        auto *page=visual(root,"shortcutsPage"),*input=visual(root,"shortcut-app-miniPlayer");QVERIFY(page);QVERIFY(input);QVERIFY(visual(root,"shortcut-global-miniPlayer"));
        QCOMPARE(visual(root,"shortcut-global-miniPlayer")->property("text").toString(),QString("Ctrl+Alt+M"));
        QCOMPARE(input->property("text").toString(),QString("Ctrl+Shift+M"));
        auto *scroll=visual(root,"settingsScroll");auto *flick=scroll->property("contentItem").value<QQuickItem *>();QVERIFY(flick);
        flick->setProperty("contentY",flick->property("contentY").toReal()+rect(input).center().y()-rect(scroll).center().y());w->grabWindow();
        shot(w,"mini-shortcut-setting");input->forceActiveFocus();QTest::keyClick(w,Qt::Key_J,Qt::ControlModifier|Qt::AltModifier);QCOMPARE(input->property("text").toString(),QString("Ctrl+Alt+J"));QVERIFY(!w->property("miniMode").toBool());
        auto *save=visual(root,"shortcutsSave");QVERIFY(save);flick->setProperty("contentY",flick->property("contentY").toReal()+rect(save).center().y()-rect(scroll).center().y());w->grabWindow();QVERIFY(click(w,save));
        root->forceActiveFocus();QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(!w->property("miniMode").toBool());
        QTest::keyClick(w,Qt::Key_J,Qt::ControlModifier|Qt::AltModifier);QVERIFY(w->property("miniMode").toBool());QVERIFY(miniSettled(w));
        QTest::keyClick(w,Qt::Key_J,Qt::ControlModifier|Qt::AltModifier);QVERIFY(miniSettled(w));QVERIFY(!w->property("miniMode").toBool());
        QCOMPARE(f.library.setting("shortcuts").toObject()["app"].toObject()["miniPlayer"].toString(),QString("Ctrl+Alt+J"));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniPlayerKeepsOneWindowAndRestoresItsState_data() {
        QTest::addColumn<int>("width");QTest::addColumn<bool>("pinned");QTest::addColumn<bool>("expanded");QTest::addColumn<int>("presentation");
        QTest::newRow("wide")<<1240<<false<<false<<int(QWindow::Windowed);
        QTest::newRow("narrow")<<414<<false<<false<<int(QWindow::Windowed);
        QTest::newRow("pinned")<<1240<<true<<false<<int(QWindow::Windowed);
        QTest::newRow("expanded")<<414<<false<<true<<int(QWindow::Windowed);
        QTest::newRow("maximized")<<1240<<true<<false<<int(QWindow::Maximized);
        QTest::newRow("fullscreen")<<1240<<false<<true<<int(QWindow::FullScreen);
    }
    void miniPlayerKeepsOneWindowAndRestoresItsState() {
        QFETCH(int,width);QFETCH(bool,pinned);QFETCH(bool,expanded);QFETCH(int,presentation);
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->playbackBridge();
        w->resize(width,790);w->setProperty("windowPinned",pinned);QTest::qWait(420);
        if(expanded){QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(480);}
        const auto normal=w->geometry();
        if(presentation==QWindow::Maximized){w->showMaximized();QTRY_COMPARE(w->visibility(),QWindow::Maximized);}
        if(presentation==QWindow::FullScreen){QVERIFY(QMetaObject::invokeMethod(w,"toggleFullscreen"));QTRY_COMPARE(w->visibility(),QWindow::FullScreen);QTest::qWait(480);}
        const auto before=w->geometry();const auto handle=w->winId();const auto key=bridge->trackId();const auto queue=f.shell->facade()->queue().items;
        const auto saved=f.shell->settingsBridge()->windowBounds();
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QTRY_VERIFY(w->property("miniMode").toBool());QVERIFY(miniSettled(w));QCOMPARE(w->size(),QSize(340,58));
        QCOMPARE(w->winId(),handle);QVERIFY(w->isVisible());QVERIFY(w->flags().testFlag(Qt::WindowStaysOnTopHint));QCOMPARE(w->property("windowPinned").toBool(),pinned);
        auto *mini=visual(root,"miniPlayer");QVERIFY(mini);QVERIFY(mini->isVisible());QVERIFY(!visual(root,"normalShell")->isVisible());QVERIFY(!visual(root,"playbackProgress")->isVisible());
        auto *cover=visual(mini,"miniCover"),*play=visual(mini,"miniPlayPause");QVERIFY(cover);QVERIFY(play);QCOMPARE(cover->size(),play->size());QCOMPARE(cover->width(),42.);
        miniHover(w,false);QVERIFY(!visual(mini,"miniControls")->isVisible());QVERIFY(visual(mini,"miniLyrics")->isVisible());QVERIFY(!visual(mini,"miniMetadata")->isVisible());
        QVERIFY(visual(mini,"miniLyric"));QVERIFY(!visual(mini,"miniTranslationViewport")->isVisible());QVERIFY(!visual(mini,"miniPausedNext"));QVERIFY(!visual(mini,"miniRestore"));
        const qreal informationCenter=(rect(cover).right()+w->width()-rect(cover).left())/2;
        for(const auto *name:{"miniTitle","miniArtist"}){
            auto *text=visual(mini,name);QVERIFY(text);QCOMPARE(text->property("horizontalAlignment").toInt(),int(Qt::AlignHCenter));
            QVERIFY(qAbs(rect(text).center().x()-informationCenter)<=1);QVERIFY(rect(text).left()>rect(cover).right());
        }
        miniHover(w,true);QVERIFY(visual(mini,"miniControls")->isVisible());QVERIFY(!visual(mini,"miniRestingInfo")->isVisible());
        for(const auto *name:{"miniCover","miniQueueButton","miniFavorite","miniPrevious","miniPlayPause","miniPlayingNext","miniPlayMode","miniVolume"}){
            auto *item=visual(mini,name);QVERIFY(item);QVERIFY2(rect(item).left()>=0&&rect(item).right()<=w->width()&&rect(item).top()>=0&&rect(item).bottom()<=w->height(),name);
            QVERIFY2(item->width()>=36&&item->height()>=36,name);
        }
        QTRY_VERIFY(bridge->seekable());bridge->seek(7000);QTRY_VERIFY(f.shell->lyricsModel()->currentIndex()>=7);
        QTRY_COMPARE(visual(mini,"miniLyric")->property("text").toString(),f.shell->lyricsModel()->textAt(f.shell->lyricsModel()->currentIndex()));
        QCOMPARE(visual(mini,"miniTitle")->property("text").toString(),bridge->title());QCOMPARE(visual(mini,"miniArtist")->property("text").toString(),bridge->artist());
        if(width==1240&&!pinned&&presentation==QWindow::Windowed)shot(w,"mini-hover-controls");
        miniHover(w,false);if(width==1240&&!pinned&&presentation==QWindow::Windowed)shot(w,"mini-playing");
        miniHover(w,true);QVERIFY(click(w,play));QTRY_VERIFY(!bridge->playing());miniHover(w,false);QVERIFY(!visual(mini,"miniControls")->isVisible());QVERIFY(visual(mini,"miniMetadata")->isVisible());
        QVERIFY(rect(visual(mini,"miniTitle")).bottom()+2<=rect(visual(mini,"miniArtist")).top());
        if(width==1240&&!pinned&&presentation==QWindow::Windowed)shot(w,"mini-paused");
        QTest::qWait(420);QCOMPARE(f.shell->settingsBridge()->windowBounds(),saved);
        miniHover(w,true);QVERIFY(click(w,play));QTRY_VERIFY(bridge->playing());
        auto *coverDrag=visual(mini,"miniCoverDrag");QVERIFY(coverDrag);w->grabWindow();QCoreApplication::processEvents();
        const auto dragStart=rect(coverDrag).center().toPoint(),position=w->position();
        const auto dragEnd=w->mapToGlobal(dragStart)+QPoint(25,15);
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,dragStart);
        QTest::mouseMove(w,w->mapFromGlobal(dragEnd),30);
        QTRY_COMPARE(w->position(),position+QPoint(25,15));
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(dragEnd));
        QQuickWindow *shortcutWindow=w;
        if(width==1240&&!pinned&&presentation==QWindow::Windowed){
            auto *volume=visual(mini,"miniVolume");QTest::mouseMove(w,rect(volume).center().toPoint());QCoreApplication::processEvents();
            auto *popup=volume->findChild<QObject *>("compactVolumePopup");QVERIFY(popup);QVERIFY(popup->property("visible").toBool());shortcutWindow=popup->property("contentItem").value<QQuickItem *>()->window();
        }
        QTest::keyClick(shortcutWindow,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QTest::qWait(180);QTRY_COMPARE(w->visibility(),QWindow::Visibility(presentation));QTRY_COMPARE(w->geometry(),before);
        QCOMPARE(w->winId(),handle);QCOMPARE(w->flags().testFlag(Qt::WindowStaysOnTopHint),pinned);QCOMPARE(w->property("lyricsOpen").toBool(),expanded);
        QCOMPARE(bridge->trackId(),key);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(bridge->playing());QVERIFY(bridge->position()>=6900);
        if(presentation==QWindow::Maximized){w->showNormal();QTRY_COMPARE(w->geometry(),normal);}
        if(presentation==QWindow::FullScreen){QVERIFY(QMetaObject::invokeMethod(w,"toggleFullscreen"));QTRY_COMPARE(w->geometry(),normal);}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniPlayerPreservesDraftsAndProvidesQueueAndVolume() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->playbackBridge();auto *settings=f.shell->settingsBridge();
        QVERIFY(f.shell->appBridge()->navigate(3));w->setProperty("settingsSection","appearance");QTest::qWait(200);settings->beginAppearance();settings->setAppearanceMode(3);QVERIFY(settings->editAppearance("rightInk","#123456"));
        const auto draft=settings->appearanceDraft();QVERIFY(QMetaObject::invokeMethod(w,"enterMiniPlayer"));QVERIFY(miniSettled(w));QCOMPARE(w->size(),QSize(340,58));
        auto *mini=visual(root,"miniPlayer");QVERIFY(mini);w->grabWindow();QTest::mouseDClick(w,Qt::LeftButton,Qt::NoModifier,rect(visual(mini,"miniCoverDrag")).center().toPoint());QTRY_VERIFY(!w->property("miniMode").toBool());QVERIFY(miniSettled(w));QTRY_COMPARE(w->size(),QSize(1240,790));
        QCOMPARE(settings->appearanceDraft(),draft);QCOMPARE(w->property("settingsSection").toString(),QString("appearance"));QCOMPARE(f.shell->appBridge()->page(),3);settings->discardAppearance();
        w->setProperty("settingsSection","settings");QVERIFY(f.shell->appBridge()->navigate(0));QTest::qWait(200);
        w->grabWindow();
        auto *create=visual(root,"sidebarCreatePlaylist");QSignalSpy createClicks(create,SIGNAL(clicked()));QVERIFY(createClicks.isValid());QVERIFY(click(w,create));QCOMPARE(createClicks.count(),1);
        auto *editor=w->findChild<QObject *>("playlistEditor");QVERIFY(editor);QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return editor->property("opened").toBool();}));
        auto *input=visual(root,"playlistNameInput");QVERIFY(input);input->setProperty("text",QString("迷你切换后保留草稿"));
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QTRY_VERIFY(w->property("miniMode").toBool());QVERIFY(miniSettled(w));mini=visual(root,"miniPlayer");QVERIFY(mini);
        miniHover(w,true);QVERIFY(click(w,visual(mini,"miniPlayPause")));QTRY_VERIFY(!bridge->playing());QVERIFY(click(w,visual(mini,"miniPlayPause")));QTRY_VERIFY(bridge->playing());
        auto *queueButton=visual(mini,"miniQueueButton");QVERIFY(click(w,queueButton));auto *popup=mini->findChild<QObject *>("miniQueuePopup");QVERIFY(popup);QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return popup->property("opened").toBool();}));
        auto *surface=popup->property("contentItem").value<QQuickItem *>();QVERIFY(surface);QVERIFY(surface->window()!=w);QVERIFY(surface->height()>w->height());
        const auto panelBottom=surface->mapToGlobal(QPointF(0,surface->height())).y();
        QCOMPARE(qRound(panelBottom),w->y());QCOMPARE(qRound(surface->mapToGlobal(QPointF()).x()),w->x());QCOMPARE(qRound(surface->width()),w->width());
        shot(surface->window(),"mini-queue");QVERIFY(click(w,queueButton));QVERIFY(!popup->property("visible").toBool());QCOMPARE(f.shell->facade()->current().id,f.a.id);
        auto *volume=visual(mini,"miniVolume");QVERIFY(volume);QTest::mouseMove(w,rect(volume).center().toPoint());QCoreApplication::processEvents();auto *volumePopup=volume->findChild<QObject *>("compactVolumePopup");QVERIFY(volumePopup);QVERIFY(volumePopup->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(volumePopup,"close"));
        QVERIFY(click(w,queueButton));QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return popup->property("opened").toBool();}));surface=popup->property("contentItem").value<QQuickItem *>();QTest::mouseClick(surface->window(),Qt::LeftButton,Qt::NoModifier,rect(surface).topLeft().toPoint()+QPoint(100,32));QTRY_COMPARE(f.shell->facade()->current().id,f.b.id);
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QTRY_VERIFY(editor->property("opened").toBool());QVERIFY(editor->property("modal").toBool());QVERIFY(editor->property("enabled").toBool());QCOMPARE(input->property("text").toString(),QString("迷你切换后保留草稿"));
        QVERIFY(QMetaObject::invokeMethod(editor,"reject"));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniPlayerWaitsForTheMinimumHeightDragThreshold_data() {
        QTest::addColumn<int>("edge");QTest::addColumn<int>("startHeight");QTest::addColumn<QPoint>("inward");
        QTest::newRow("bottom-at-minimum")<<int(Qt::BottomEdge)<<540<<QPoint(0,-1);
        QTest::newRow("top-at-minimum")<<int(Qt::TopEdge)<<540<<QPoint(0,1);
        QTest::newRow("bottom-right")<<int(Qt::BottomEdge|Qt::RightEdge)<<580<<QPoint(-1,-1);
        QTest::newRow("top-left")<<int(Qt::TopEdge|Qt::LeftEdge)<<580<<QPoint(1,1);
    }
    void miniPlayerWaitsForTheMinimumHeightDragThreshold() {
        QFETCH(int,edge);QFETCH(int,startHeight);QFETCH(QPoint,inward);
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        const auto key=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        // Programmatic shrinking and reaching the minimum are not gestures.
        w->resize(460,540);QTest::qWait(400);w->grabWindow();QVERIFY(!w->property("miniMode").toBool());
        w->resize(460,startHeight);QTest::qWait(100);w->grabWindow();QCoreApplication::processEvents();
        auto *resizeEdge=visual(root,QString("windowResizeEdge-%1").arg(edge));QVERIFY(resizeEdge);
        const auto start=rect(resizeEdge).center().toPoint(),global=w->mapToGlobal(start);
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);
        QVERIFY(resizeEdge->property("compactResize").toBool());
        const int toMinimum=startHeight-540;
        QTest::mouseMove(w,w->mapFromGlobal(global+inward*(toMinimum+32)),30);
        QVERIFY(!w->property("miniMode").toBool());QCOMPARE(w->height(),540);
        const auto minimumGeometry=w->geometry();
        QSignalSpy heights(w,&QWindow::heightChanged);
        QTest::mouseMove(w,w->mapFromGlobal(global+inward*(toMinimum+80)),30);
        QVERIFY(w->property("miniMode").toBool());
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(global+inward*(toMinimum+80)));
        QVERIFY(miniSettled(w));QCOMPARE(w->size(),QSize(340,58));
        int intermediate=0;for(const auto &args:heights)if(args[0].toInt()>58&&args[0].toInt()<540)++intermediate;
        QVERIFY2(intermediate>=2,qPrintable(QString("Only %1 intermediate resize frames among %2 height changes").arg(intermediate).arg(heights.size())));
        if(edge&Qt::TopEdge)QCOMPARE(w->geometry().bottom(),minimumGeometry.bottom());
        QVERIFY(QMetaObject::invokeMethod(w,"leaveMiniPlayer"));QVERIFY(miniSettled(w));QTRY_COMPARE(w->geometry(),minimumGeometry);
        QCOMPARE(f.shell->facade()->current().id,key);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void wideWindowCornersEnterMiniPlayer_data() {
        QTest::addColumn<int>("edge");QTest::addColumn<QPoint>("direction");
        QTest::newRow("top-left")<<int(Qt::TopEdge|Qt::LeftEdge)<<QPoint(1,1);
        QTest::newRow("top-right")<<int(Qt::TopEdge|Qt::RightEdge)<<QPoint(-1,1);
        QTest::newRow("bottom-left")<<int(Qt::BottomEdge|Qt::LeftEdge)<<QPoint(1,-1);
        QTest::newRow("bottom-right")<<int(Qt::BottomEdge|Qt::RightEdge)<<QPoint(-1,-1);
    }
    void wideWindowCornersEnterMiniPlayer() {
        QFETCH(int,edge);QFETCH(QPoint,direction);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        w->resize(900,700);w->grabWindow();QTest::qWait(350);w->grabWindow();
        const auto original=w->geometry();const auto handle=w->winId();const auto current=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        auto *corner=visual(root,QString("windowResizeEdge-%1").arg(edge));QVERIFY(corner);
        const auto start=rect(corner).center().toPoint(),global=w->mapToGlobal(start);
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);QVERIFY(corner->property("compactResize").toBool());
        // Pulling just the height of a wide window does not count as an inward corner gesture.
        QTest::mouseMove(w,w->mapFromGlobal(global+QPoint(0,direction.y()*240)),30);
        QVERIFY(!w->property("miniMode").toBool());QCOMPARE(w->size(),QSize(900,540));
        QTest::mouseMove(w,w->mapFromGlobal(global+QPoint(direction.x()*100,direction.y()*240)),30);
        QVERIFY(w->property("miniMode").toBool());
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(global+QPoint(direction.x()*100,direction.y()*240)));
        QVERIFY(miniSettled(w));QCOMPARE(w->size(),QSize(340,58));QCOMPARE(w->winId(),handle);
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));
        QCOMPARE(w->geometry(),QRect(original.x()+((edge&Qt::LeftEdge)?100:0),original.y()+((edge&Qt::TopEdge)?160:0),800,540));
        QCOMPARE(f.shell->facade()->current().id,current);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniQueueIsFlushAboveAndClosesOnPointerLeave_data() {
        QTest::addColumn<bool>("nearTop");QTest::newRow("top-edge")<<true;QTest::newRow("ample-space")<<false;
    }
    void miniQueueIsFlushAboveAndClosesOnPointerLeave() {
        QFETCH(bool,nearTop);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        QVector<qint64> ids{f.a.id,f.b.id};
        for(int i=0;i<5;++i){const auto track=seed(f.library,QString("队列歌曲 %1 Long title 中文 English").arg(i),QString("mini-queue-%1").arg(i));QVERIFY(track.id);QVERIFY(wave(f.library.absolute(track.file)));ids.append(track.id);}
        f.shell->facade()->play(ids,f.a.id);QTRY_VERIFY(f.shell->upcomingModel()->count()>=5);
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));
        const auto screen=w->screen()->geometry();w->setPosition(screen.left()+80,screen.top()+(nearTop?8:420));miniHover(w,true);
        auto *mini=visual(root,"miniPlayer"),*button=visual(mini,"miniQueueButton");QVERIFY(button);
        auto *popup=mini->findChild<QObject *>("miniQueuePopup");QVERIFY(popup);QSignalSpy reveal(popup,SIGNAL(revealChanged()));QVERIFY(reveal.isValid());
        QVERIFY(click(w,button));
        QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return popup->property("opened").toBool();}));
        auto *surface=popup->property("contentItem").value<QQuickItem *>();QVERIFY(surface);auto *floating=surface->window();QVERIFY(floating!=w);floating->grabWindow();
        QCOMPARE(qRound(surface->width()),340);QCOMPARE(qRound(surface->height()),288);
        QCOMPARE(qRound(surface->mapToGlobal(QPointF()).x()),w->x());QCOMPARE(qRound(surface->mapToGlobal(QPointF(0,surface->height())).y()),w->y());
        QVERIFY(floating->y()>=screen.top());QVERIFY(reveal.count()>=2);QCOMPARE(visual(surface,"miniQueueReveal")->height(),288.);
        auto *panel=visual(surface,"miniUpcomingPanel"),*rows=visual(surface,"upcomingRows");QVERIFY(panel);QVERIFY(rows);QCOMPARE(rows->height(),280.);
        auto *backdrop=visual(surface,"miniQueueBackdrop"),*glass=visual(surface,"miniQueueGlass");QVERIFY(backdrop);QVERIFY(glass);
        QVERIFY(backdrop->property("source").toUrl().toString().startsWith("image://relaxer/blur?"));QVERIFY(glass->property("liquid").toBool());
        QCOMPARE(QUrl(QUrlQuery(backdrop->property("source").toUrl()).queryItemValue("file",QUrl::FullyDecoded)),f.shell->playbackBridge()->coverUrl());
        QVERIFY(!visual(surface,"nextQueueHeading")->isVisible());QVERIFY(!visual(surface,"upcomingToggle")->isVisible());
        for(int i=0;i<5;++i){auto *title=visual(rows,QString("upcomingTitle-%1").arg(i));QVERIFY(title);QVERIFY(rect(title).top()>=rect(rows).top());QVERIFY(rect(title).bottom()<=rect(rows).bottom());}
        shot(floating,QString("mini-queue-flush-%1").arg(nearTop));
        QEvent barLeave(QEvent::Leave);QCoreApplication::sendEvent(w,&barLeave);
        const QPoint center=rect(surface).center().toPoint();QEnterEvent enter(center,center,floating->mapToGlobal(center));QCoreApplication::sendEvent(floating,&enter);
        QTest::mouseMove(floating,center);QCoreApplication::processEvents();QVERIFY(popup->property("visible").toBool());
        QEvent listLeave(QEvent::Leave);QCoreApplication::sendEvent(floating,&listLeave);
        QVERIFY(!popup->property("visible").toBool());
        QCOMPARE(f.shell->facade()->queue().items,ids);QCOMPARE(f.shell->facade()->current().id,f.a.id);QVERIFY(f.shell->facade()->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void miniEdgesResistThenExpandWithoutResettingPlayback_data() {
        QTest::addColumn<int>("edge");QTest::addColumn<QPoint>("travel");QTest::addColumn<bool>("pinned");QTest::addColumn<bool>("reduceMotion");
        QTest::newRow("bottom-narrow")<<int(Qt::BottomEdge)<<QPoint(0,482)<<false<<false;
        QTest::newRow("top-narrow")<<int(Qt::TopEdge)<<QPoint(0,-482)<<false<<false;
        QTest::newRow("left-wide")<<int(Qt::LeftEdge)<<QPoint(-240,0)<<false<<false;
        QTest::newRow("right-wide-pinned")<<int(Qt::RightEdge)<<QPoint(240,0)<<true<<false;
        QTest::newRow("top-left-narrow")<<int(Qt::TopEdge|Qt::LeftEdge)<<QPoint(-80,-482)<<true<<false;
        QTest::newRow("top-right-narrow")<<int(Qt::TopEdge|Qt::RightEdge)<<QPoint(80,-482)<<false<<false;
        QTest::newRow("bottom-left-narrow")<<int(Qt::BottomEdge|Qt::LeftEdge)<<QPoint(-80,482)<<false<<false;
        QTest::newRow("bottom-right-narrow")<<int(Qt::BottomEdge|Qt::RightEdge)<<QPoint(80,482)<<false<<false;
        QTest::newRow("top-left-wide")<<int(Qt::TopEdge|Qt::LeftEdge)<<QPoint(-240,-482)<<false<<false;
        QTest::newRow("top-right-wide")<<int(Qt::TopEdge|Qt::RightEdge)<<QPoint(240,-482)<<false<<false;
        QTest::newRow("bottom-left-wide")<<int(Qt::BottomEdge|Qt::LeftEdge)<<QPoint(-240,482)<<false<<false;
        QTest::newRow("bottom-right-wide")<<int(Qt::BottomEdge|Qt::RightEdge)<<QPoint(240,482)<<false<<false;
        QTest::newRow("no-animation")<<int(Qt::BottomEdge)<<QPoint(0,482)<<false<<true;
    }
    void miniEdgesResistThenExpandWithoutResettingPlayback() {
        QFETCH(int,edge);QFETCH(QPoint,travel);QFETCH(bool,pinned);QFETCH(bool,reduceMotion);
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->playbackBridge();
        w->setProperty("windowPinned",pinned);QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",reduceMotion));
        const auto handle=w->winId();const auto key=bridge->trackId();const auto queue=f.shell->facade()->queue().items;
        const auto saved=f.shell->settingsBridge()->windowBounds();
        QTest::keyClick(w,Qt::Key_M,Qt::ControlModifier|Qt::ShiftModifier);QVERIFY(miniSettled(w));QCOMPARE(w->size(),QSize(340,58));
        const auto initial=w->geometry();auto *resizeEdge=visual(root,QString("miniResizeEdge-%1").arg(edge));QVERIFY(resizeEdge);
        auto start=rect(resizeEdge).center().toPoint(),global=w->mapToGlobal(start);
        const auto partial=travel/4;
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);QVERIFY(w->property("miniResizeActive").toBool());
        QTest::mouseMove(w,w->mapFromGlobal(global+partial),30);
        QVERIFY(w->property("miniMode").toBool());QVERIFY(w->size()!=QSize(340,58));
        QVERIFY(w->width()>=340&&w->width()<=364);QVERIFY(w->height()>=58&&w->height()<=72);
        if(edge&Qt::LeftEdge)QCOMPARE(w->geometry().right(),initial.right());
        if(edge&Qt::TopEdge)QCOMPARE(w->geometry().bottom(),initial.bottom());
        shot(w,QString("mini-elastic-%1").arg(edge));
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(global+partial));
        QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return w->geometry()==initial;}));
        QCOMPARE(f.shell->settingsBridge()->windowBounds(),saved);
        w->grabWindow();start=rect(resizeEdge).center().toPoint();global=w->mapToGlobal(start);
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(w,w->mapFromGlobal(global+travel*3/4),30);QVERIFY(w->property("miniMode").toBool());
        QSignalSpy heights(w,&QWindow::heightChanged);
        QTest::mouseMove(w,w->mapFromGlobal(global+travel),30);QVERIFY(!w->property("miniMode").toBool());
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,w->mapFromGlobal(global+travel));
        QVERIFY(miniSettled(w));
        int intermediate=0;for(const auto &args:heights)if(args[0].toInt()>72&&args[0].toInt()<540)++intermediate;
        if(!reduceMotion)QVERIFY2(intermediate>=2,qPrintable(QString("Only %1 intermediate resize frames among %2 height changes").arg(intermediate).arg(heights.size())));
        QCOMPARE(w->size(),QSize(qMax(414,340+qAbs(travel.x())),qMax(540,58+qAbs(travel.y()))));
        QCOMPARE(w->property("narrowLayout").toBool(),w->width()<547);QCOMPARE(w->winId(),handle);
        QCOMPARE(w->flags().testFlag(Qt::WindowStaysOnTopHint),pinned);QCOMPARE(w->property("windowPinned").toBool(),pinned);
        QCOMPARE(bridge->trackId(),key);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(bridge->playing());
        QVERIFY(visual(root,"normalShell")->isEnabled());QVERIFY(!visual(root,"miniPlayer"));
        shot(w,QString("mini-expanded-%1").arg(edge));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
#ifdef Q_OS_WIN
    void nativeWindowShadowFollowsForeground() {
        // Actual desktop activation/capture is opt-in, so ordinary layout runs
        // do not depend on owning the user's foreground window.
        if(!qEnvironmentVariableIntValue("RELAXER_INTERACTIVE_TESTS"))QSKIP("Set RELAXER_INTERACTIVE_TESTS=1 for native desktop shadow verification");
        const auto origin=QGuiApplication::primaryScreen()->availableGeometry().topLeft();
        const QRect geometry(origin+QPoint(80,80),QSize(700,540));
        QQuickWindow backdrop;backdrop.setColor(Qt::white);backdrop.setGeometry(geometry.adjusted(-48,-48,48,48));backdrop.show();
        QVERIFY(QTest::qWaitForWindowExposed(&backdrop));
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();w->setGeometry(geometry);w->raise();w->requestActivate();
        QTRY_VERIFY(w->isActive());QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(),reinterpret_cast<HWND>(w->winId()),20000);
        const auto window=reinterpret_cast<HWND>(w->winId());
        const auto rendering=[&]{BOOL enabled=FALSE;return SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_NCRENDERING_ENABLED,&enabled,sizeof(enabled)))&&enabled;};
        QTRY_VERIFY(rendering());
        const QRect capture=geometry.adjusted(-32,-32,32,32);
        const auto grab=[&]{DwmFlush();QTest::qWait(120);return w->screen()->grabWindow(0,capture.x(),capture.y(),capture.width(),capture.height()).toImage();};
        const auto active=grab();QVERIFY(!active.isNull());
        QQuickWindow peer;peer.setFlags(Qt::Window|Qt::FramelessWindowHint);peer.setGeometry(QRect(origin+QPoint(4,4),QSize(30,30)));peer.setColor(Qt::white);peer.show();peer.requestActivate();
        QTRY_VERIFY(peer.isActive()&&!w->isActive());QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(),reinterpret_cast<HWND>(peer.winId()),20000);QTRY_VERIFY(!rendering());
        const auto inactive=grab();QVERIFY(!inactive.isNull());
        const auto outsideDarkness=[&](const QImage &image){
            const qreal scale=image.devicePixelRatio();double sum=0;int count=0;
            for(int y=qRound(64*scale);y<qRound((capture.height()-64)*scale);++y)
                for(int x=qRound((32+geometry.width()+2)*scale);x<qRound((32+geometry.width()+20)*scale);++x){sum+=255-qGray(image.pixel(x,y));++count;}
            return sum/count;
        };
        const auto folder=qEnvironmentVariable("RELAXER_ADAPTIVE_SCREENSHOTS");
        if(!folder.isEmpty()){QDir().mkpath(folder);active.save(QDir(folder).filePath("native-shadow-active.png"));inactive.save(QDir(folder).filePath("native-shadow-inactive.png"));}
        const double on=outsideDarkness(active),off=outsideDarkness(inactive);qInfo()<<"outside shadow darkness"<<on<<off;
        QVERIFY2(on>off+.5,"Native rendering was enabled but no outside shadow was painted");
        QCOMPARE(w->geometry(),geometry);QVERIFY(f.shell->facade()->playing());
        QVERIFY(w->setProperty("windowPinned",true));QVERIFY(!w->isActive());QVERIFY(!rendering());
        w->requestActivate();QTRY_VERIFY(w->isActive());QTRY_VERIFY(rendering());
        QVERIFY(w->setProperty("windowPinned",false));QTRY_VERIFY(rendering());
        w->showMaximized();QTRY_COMPARE(w->visibility(),QWindow::Maximized);QTRY_VERIFY(!rendering());
        w->showNormal();QTRY_COMPARE(w->visibility(),QWindow::Windowed);QTRY_VERIFY(rendering());
        w->showFullScreen();QTRY_COMPARE(w->visibility(),QWindow::FullScreen);QTRY_VERIFY(!rendering());
        w->showNormal();QTRY_COMPARE(w->visibility(),QWindow::Windowed);QTRY_VERIFY(rendering());
        QCOMPARE(w->geometry(),geometry);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
#endif
    void windowPinKeepsTheSameWindowAndPlayback_data() {
        QTest::addColumn<int>("width");QTest::addColumn<bool>("expanded");
        QTest::newRow("wide")<<1240<<false;QTest::newRow("narrow")<<414<<false;
        QTest::newRow("wide-expanded")<<1240<<true;QTest::newRow("narrow-expanded")<<414<<true;
    }
    void windowPinKeepsTheSameWindowAndPlayback() {
        QFETCH(int,width);QFETCH(bool,expanded);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        w->resize(width,790);QTest::qWait(420);
        if(expanded){QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_COMPARE(w->property("playerProgress").toReal(),1.);QTest::qWait(100);}
        auto *pin=visual(root,"windowPinButton"),*icon=visual(root,"windowPinIcon"),*minimize=visual(root,"windowControl0");QVERIFY(pin);QVERIFY(icon);QVERIFY(minimize);
        QVERIFY(qAbs(rect(pin).right()-rect(minimize).left())<.1);QVERIFY(rect(pin).left()>=0);QCOMPARE(pin->height(),minimize->height());
        const auto geometry=w->geometry();const auto handle=w->winId();const auto current=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        const auto before=f.shell->facade()->position();
#ifdef Q_OS_WIN
        const auto nativePinned=[&]{return (GetWindowLongPtrW(reinterpret_cast<HWND>(w->winId()),GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;};
        QVERIFY(!nativePinned());
#endif
        QVERIFY(!w->property("windowPinned").toBool());QVERIFY(!w->flags().testFlag(Qt::WindowStaysOnTopHint));QCOMPARE(icon->rotation(),45.);
        shot(w,QString("window-pin-%1-%2-off").arg(width).arg(expanded));
        QVERIFY(click(w,pin));QVERIFY(w->property("windowPinned").toBool());QVERIFY(pin->property("checked").toBool());QVERIFY(w->flags().testFlag(Qt::WindowStaysOnTopHint));
        QTest::qWait(60);QVERIFY(icon->rotation()>0&&icon->rotation()<45);QTRY_COMPARE(icon->rotation(),0.);
#ifdef Q_OS_WIN
        QTRY_VERIFY(nativePinned());
#endif
        QCOMPARE(w->winId(),handle);QCOMPARE(w->visibility(),QWindow::Windowed);QCOMPARE(w->geometry(),geometry);
        shot(w,QString("window-pin-%1-%2-on").arg(width).arg(expanded));
        if(!expanded&&width==1240){
            auto *maximize=visual(root,"windowControl1");QVERIFY(click(w,maximize));QTRY_COMPARE(w->visibility(),QWindow::Maximized);QVERIFY(w->property("windowPinned").toBool());
            QVERIFY(click(w,maximize));QTRY_COMPARE(w->visibility(),QWindow::Windowed);QCOMPARE(w->geometry(),geometry);
            QVERIFY(click(w,minimize));QTRY_COMPARE(w->visibility(),QWindow::Minimized);QVERIFY(w->flags().testFlag(Qt::WindowStaysOnTopHint));
            w->showNormal();QVERIFY(QTest::qWaitForWindowExposed(w));
        }
        QVERIFY(click(w,pin));QVERIFY(!w->property("windowPinned").toBool());QVERIFY(!pin->property("checked").toBool());QVERIFY(!w->flags().testFlag(Qt::WindowStaysOnTopHint));
        QTest::qWait(60);QVERIFY(icon->rotation()>0&&icon->rotation()<45);QTRY_COMPARE(icon->rotation(),45.);
#ifdef Q_OS_WIN
        QTRY_VERIFY(!nativePinned());
#endif
        QCOMPARE(w->winId(),handle);QCOMPARE(w->geometry(),geometry);QVERIFY(w->isVisible());
        QCOMPARE(w->property("lyricsOpen").toBool(),expanded);QCOMPARE(f.shell->facade()->current().id,current);QCOMPARE(f.shell->facade()->queue().items,queue);
        QVERIFY(f.shell->facade()->playing());QVERIFY(f.shell->facade()->position()>=before-100);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void narrowVolumeUsesImmediateContinuousHoverAndClickMute_data() {
        QTest::addColumn<int>("width");QTest::addColumn<bool>("expanded");QTest::addColumn<bool>("screenEdge");
        QTest::newRow("minimum-footer")<<414<<false<<false;QTest::newRow("narrow-footer")<<546<<false<<false;
        QTest::newRow("minimum-expanded")<<414<<true<<false;QTest::newRow("narrow-expanded")<<546<<true<<false;
        QTest::newRow("screen-edge-footer")<<414<<false<<true;
    }
    void narrowVolumeUsesImmediateContinuousHoverAndClickMute() {
        QFETCH(int,width);QFETCH(bool,expanded);QFETCH(bool,screenEdge);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->playbackBridge();
        bridge->pause();bridge->setVolume(37);const auto current=bridge->trackId();const auto queue=f.shell->facade()->queue().items;
        w->resize(width,790);QTest::qWait(420);
        if(screenEdge){const auto screen=w->screen()->availableGeometry();w->setPosition(screen.right()+1-width,screen.bottom()+1-w->height()-20);QTest::qWait(100);}
        if(expanded){QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_COMPARE(w->property("playerProgress").toReal(),1.);QTest::qWait(100);}
        auto *speaker=visual(root,"muteButton");QVERIFY(speaker);auto *popup=w->findChild<QObject *>("compactVolumePopup");QVERIFY(popup);
        // QtTest sends moves directly to the chosen window; supply the leave
        // event Windows normally sends when moving to another native surface.
        QQuickWindow *hoverWindow=nullptr;
        const auto move=[&](QQuickWindow *window,QPoint point){
            if(hoverWindow&&hoverWindow!=window){QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(hoverWindow,&leave);}
            QTest::mouseMove(window,point);hoverWindow=window;QCoreApplication::processEvents();
        };
        move(w,QPoint(200,300));QVERIFY(!popup->property("visible").toBool());
        move(w,rect(speaker).center().toPoint());QVERIFY(popup->property("opened").toBool());
        auto *hover=popup->property("contentItem").value<QQuickItem *>();QVERIFY(hover);auto *floating=hover->window();QVERIFY(floating);QVERIFY(floating!=w);
        auto *panel=visual(hover,"compactVolumePanel"),*slider=visual(hover,"compactVolumeSlider"),*mute=visual(hover,"compactVolumeMuteButton");QVERIFY(panel);QVERIFY(slider);QVERIFY(mute);
        const auto speakerCenter=speaker->mapToGlobal(QPointF(speaker->width()/2,speaker->height()/2));
        const auto panelCenter=panel->mapToGlobal(QPointF(panel->width()/2,panel->height()/2));QVERIFY(qAbs(speakerCenter.x()-panelCenter.x())<=1);
        const auto muteCenter=mute->mapToGlobal(QPointF(18,18));QVERIFY2(qAbs(muteCenter.x()-speakerCenter.x())<=1&&qAbs(muteCenter.y()-speakerCenter.y())<=1,qPrintable(QString("Speaker (%1,%2), popup (%3,%4)").arg(speakerCenter.x()).arg(speakerCenter.y()).arg(muteCenter.x()).arg(muteCenter.y())));QVERIFY(panelCenter.y()<speakerCenter.y());
        QCOMPARE(panel->childItems().size(),1);
        if(screenEdge)QVERIFY(hover->width()<102);
        // Corners and the full-width gap belong to the same hover rectangle.
        const int right=qRound(hover->width())-2;
        for(const auto &point:QList<QPoint>{QPoint(1,1),QPoint(right,1),QPoint(right,46),QPoint(1,46),QPoint(1,84),QPoint(right,84)}) {
            move(floating,hover->mapToScene(point).toPoint());QVERIFY(popup->property("opened").toBool());
        }
        QVERIFY(click(floating,mute));QCOMPARE(bridge->volume(),0);QVERIFY(bridge->muted());QCOMPARE(mute->property("symbol").toString(),QString("volumeMuted"));
        QVERIFY(click(floating,mute));QCOMPARE(bridge->volume(),37);QVERIFY(!bridge->muted());
        auto *handle=slider->property("handle").value<QQuickItem *>();QVERIFY(handle);const auto track=rect(slider);const QPoint start=rect(handle).center().toPoint();
        QTest::mousePress(floating,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(floating,QPoint(qRound(track.left()),start.y()),30);QTest::mouseRelease(floating,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(track.left()),start.y()));
        QCOMPARE(bridge->volume(),0);QVERIFY(bridge->muted());
        QVERIFY(click(floating,mute));QCOMPARE(bridge->volume(),0);QVERIFY(!bridge->muted());QCOMPARE(mute->property("symbol").toString(),QString("volumeZero"));
        shot(floating,QString("volume-hover-%1-%2%3").arg(width).arg(expanded).arg(screenEdge?"-screen-edge":""));
        move(floating,QPoint(-2,43));QVERIFY(!popup->property("visible").toBool());
        move(w,rect(speaker).center().toPoint());QVERIFY(popup->property("opened").toBool());
        move(w,QPoint(200,300));QVERIFY(!popup->property("visible").toBool());
        w->resize(1240,790);QTest::qWait(420);move(w,rect(speaker).center().toPoint());QVERIFY(!popup->property("visible").toBool());
        bridge->setVolume(43);QVERIFY(click(w,speaker));QCOMPARE(bridge->volume(),0);QVERIFY(click(w,speaker));QCOMPARE(bridge->volume(),43);
        QCOMPARE(bridge->trackId(),current);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(!bridge->playing());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void expandedPlayerBlocksCoveredControls_data() {
        QTest::addColumn<int>("width");QTest::newRow("wide")<<1240;QTest::newRow("near-narrow")<<547;QTest::newRow("narrow")<<414;
    }
    void expandedPlayerBlocksCoveredControls() {
        QFETCH(int,width);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *playback=f.shell->playbackBridge();
        const auto key=f.shell->libraryBridge()->createPlaylist("不能穿透点击的歌单");QTest::qWait(180);w->resize(width,790);QTest::qWait(400);
        playback->pause();QTRY_VERIFY(!playback->playing());QTRY_VERIFY(playback->seekable());playback->seek(12000);QTRY_VERIFY(qAbs(playback->position()-12000)<100);
        const auto track=f.shell->facade()->current().id,position=playback->position();const auto queue=f.shell->facade()->queue().items;const auto favorite=f.library.track(f.a.id).favorite;
        QList<QPoint> points;
        for(const auto &name:QStringList{"librarySearchButton","listPlayModeMenuButton","playAllButton","trackFavorite-"+QString::number(f.a.id)}) {
            auto *item=visual(root,name);if(item&&item->isVisible())points.append(rect(item).center().toPoint());
        }
        if(width>=547)for(const auto &name:QStringList{"sidebarCreatePlaylist","navigation1","navigation4","sidebarPlaylist-"+key}) {auto *item=visual(root,name);QVERIFY(item);points.append(rect(item).center().toPoint());}
        QVERIFY(!points.isEmpty());QVERIFY(QMetaObject::invokeMethod(w,"ensurePlaylistEditor"));auto *editor=w->findChild<QObject *>("playlistEditor");QVERIFY(editor);
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,points.first());QVERIFY(!w->property("popupOpen").toBool());
        QTest::qWait(120);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,points.last());QVERIFY(!editor->property("visible").toBool());
        QTRY_COMPARE(w->property("playerProgress").toReal(),1.);QTRY_VERIFY(!w->property("coverTravelling").toBool());
        QVERIFY(visual(root,"expandedPlayerInputBarrier")->isVisible());QVERIFY(!visual(root,"paperSurface")->isEnabled());
        for(const auto &point:points) {
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,point);
            QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,point);
            QTest::mouseDClick(w,Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(25);
            QVERIFY(w->property("lyricsOpen").toBool());QCOMPARE(f.shell->appBridge()->page(),0);QCOMPARE(f.shell->libraryBridge()->playlistId(),QString("0"));
            QCOMPARE(f.shell->facade()->current().id,track);QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(qAbs(playback->position()-position)<100);QVERIFY(!playback->playing());
            QVERIFY(!w->property("popupOpen").toBool());QVERIFY(!editor->property("visible").toBool());QCOMPARE(f.library.track(f.a.id).favorite,favorite);
        }
        shot(w,QString("expanded-input-blocked-%1").arg(width));
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTest::qWait(100);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,points.last());
        QVERIFY(!w->property("popupOpen").toBool());QVERIFY(!editor->property("visible").toBool());QTRY_COMPARE(w->property("playerProgress").toReal(),0.);
        QVERIFY(visual(root,"paperSurface")->isEnabled());QVERIFY(!visual(root,"expandedPlayerInputBarrier")->isVisible());
        w->grabWindow();
        if(width>=547){QVERIFY(click(w,visual(root,"sidebarCreatePlaylist")));QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return editor->property("opened").toBool();}));}
        else{QVERIFY(click(w,visual(root,"librarySearchButton")));QVERIFY(QTest::qWaitFor([&]{w->grabWindow();return w->findChild<QObject *>("librarySearchPopup")->property("opened").toBool();}));}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void expandedPlayerKeepsVisiblePlaybackAndLyricsControlsWorking_data() {
        QTest::addColumn<int>("width");QTest::newRow("wide")<<1240;QTest::newRow("narrow")<<414;
    }
    void expandedPlayerKeepsVisiblePlaybackAndLyricsControlsWorking() {
        QFETCH(int,width);Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *playback=f.shell->playbackBridge();
        w->resize(width,790);QTest::qWait(420);QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_COMPARE(w->property("playerProgress").toReal(),1.);QTest::qWait(80);
        QVERIFY(click(w,visual(root,"playPauseButton")));QTRY_VERIFY(!playback->playing());
        auto *panel=visual(root,"lyricsPanel"),*list=visual(root,"lyricsList");QVERIFY(panel);QVERIFY(list);QTRY_VERIFY(f.shell->lyricsModel()->timed());
        wheel(w,rect(list).center().toPoint(),-120);QTRY_VERIFY(panel->property("manualScroll").toBool());
        // Entering manual mode adds the status row and resizes this viewport.
        // Measure the next wheel step after that reflow has settled.
        QTest::qWait(120);const auto initial=list->property("contentY").toReal();wheel(w,rect(list).center().toPoint(),-120);
        QTRY_VERIFY2(list->property("contentY").toReal()>initial+10,qPrintable(QString("lyrics initial=%1 now=%2 height=%3 content=%4").arg(initial).arg(list->property("contentY").toReal()).arg(list->height()).arg(list->property("contentHeight").toReal())));
        auto *time=visual(root,"lyricsPreviewTime");QVERIFY(time);QVERIFY(time->isVisible());const auto preview=panel->property("previewIndex").toInt();QVERIFY(preview>=0);
        const auto target=f.shell->lyricsModel()->timestampAt(preview);QVERIFY(click(w,time));QTRY_VERIFY(qAbs(playback->position()-target)<150);QVERIFY(!panel->property("manualScroll").toBool());
        QVERIFY(click(w,visual(root,"currentFavoriteButton")));QTRY_VERIFY(playback->currentFavorite());
        QVERIFY(click(w,visual(root,"nextButton")));QTRY_COMPARE(f.shell->facade()->current().id,f.b.id);
        QVERIFY(click(w,visual(root,"previousButton")));QTRY_COMPARE(f.shell->facade()->current().id,f.a.id);
        QQuickItem *queueSurface=nullptr;
        if(width<547){QVERIFY(click(w,visual(root,"compactQueueButton")));QTest::qWait(300);QVERIFY(w->findChild<QObject *>("narrowQueuePopup")->property("opened").toBool());queueSurface=visual(root,"narrowQueueSurface");}
        else{queueSurface=visual(root,"nextQueuePanel");QVERIFY(click(w,visual(queueSurface,"upcomingToggle")));QTest::qWait(350);QVERIFY(queueSurface->property("panelExpanded").toBool());}
        QVERIFY(queueSurface);QVERIFY(click(w,visual(queueSurface,"upcomingDelegate")));QTRY_COMPARE(f.shell->facade()->current().id,f.b.id);QTRY_VERIFY(!w->property("coverTravelling").toBool());
        QVERIFY(click(w,visual(root,width<547?"compactPlayerCollapse":"playerCollapse")));QTRY_VERIFY(!w->property("lyricsOpen").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void playModeArrowsRotateClockwiseForEveryMenuClosingPath() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *library=f.shell->libraryBridge();
        const auto key=library->createPlaylist("箭头交互歌单");QVERIFY(library->addTracksToPlaylist(key,{QString::number(f.a.id),QString::number(f.b.id)}));
        int closingPath=0;
        for(const auto &playlist:QStringList{"0","-1",key}) {
            if(playlist=="-1")QVERIFY(library->applyFavorite({QString::number(f.a.id)},true));
            QVERIFY(library->openPlaylist(playlist));QTest::qWait(200);
            for(int width:{1240,414}) {
                w->resize(width,790);QTest::qWait(400);auto *mode=visual(root,playlist==key?"playlistMode":"listPlayMode");QVERIFY(mode);QVERIFY(mode->isVisible());
                auto *button=visual(mode,"listPlayModeMenuButton"),*arrow=visual(mode,"playModeArrow");QVERIFY(button);QVERIFY(arrow);
                auto *menu=mode->findChild<QObject *>("listPlayModes");QVERIFY(menu);const auto before=arrow->rotation();
                QVERIFY(click(w,button));QTest::qWait(60);QVERIFY(menu->property("visible").toBool());QVERIFY(arrow->rotation()>before&&arrow->rotation()<before+180);
                if(closingPath!=5)QTRY_COMPARE(arrow->rotation(),before+180);
                const auto last=arrow->rotation();
                if(closingPath==0)QVERIFY(click(w,visual(root,"listModeOption-0")));
                else if(closingPath==1||closingPath==5)QVERIFY(click(w,button));
                else if(closingPath==2)QTest::keyClick(w,Qt::Key_Escape);
                else if(closingPath==3)QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(10,w->height()/2));
                else QVERIFY(click(w,visual(mode,"listPlayModeStart")));
                QTest::qWait(35);QVERIFY(arrow->rotation()>=last);QTRY_VERIFY(!menu->property("visible").toBool());QTRY_COMPARE(arrow->rotation(),before+360);
                ++closingPath;
            }
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void rightQueueKeepsItsWidthAndShowsTheNextIconWhenCollapsed() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        const auto queue=f.shell->facade()->queue().items;
        for(int track=0;track<2;++track) {
            if(track){f.shell->playbackBridge()->next();QTRY_COMPARE(f.shell->facade()->current().id,f.b.id);QTest::qWait(160);}
            const auto current=f.shell->facade()->current().id;
            for(int width:{1240,970,900,700,547}) {
                w->resize(width,790);QTest::qWait(420);auto *panel=visual(root,"nextQueuePanel");QVERIFY(panel);QVERIFY(!panel->property("panelExpanded").toBool());
                const auto collapsed=rect(panel);auto *icon=visual(panel,"nextCompactIcon"),*heading=visual(panel,"nextQueueHeading");QVERIFY(icon);QVERIFY(heading);
                QVERIFY(icon->isVisible());QVERIFY(!heading->isVisible());QCOMPARE(icon->property("name").toString(),QString("next"));
                QVERIFY(visual(panel,"nextCompactTitle")->width()>0);
                QVERIFY(rect(visual(panel,"nextCompactTitle")).right()<=rect(visual(panel,"upcomingToggle")).left()+.1);
                if(!track)shot(w,QString("queue-rc-collapsed-%1").arg(width));
                QVERIFY(click(w,visual(panel,"upcomingToggle")));QTest::qWait(350);QVERIFY(panel->property("panelExpanded").toBool());
                const auto expanded=rect(panel);QVERIFY(qAbs(expanded.width()-collapsed.width())<.1);QVERIFY(qAbs(expanded.left()-collapsed.left())<.1);QVERIFY(qAbs(expanded.right()-collapsed.right())<.1);
                QVERIFY(!icon->isVisible());QVERIFY(heading->isVisible());QCOMPARE(heading->property("text").toString(),QString("下一首"));QVERIFY(qAbs(rect(heading).left()-rect(icon).left())<.1);
                if(!track)shot(w,QString("queue-rc-expanded-%1").arg(width));
                QVERIFY(click(w,visual(panel,"upcomingToggle")));QTest::qWait(350);QVERIFY(qAbs(rect(panel).width()-collapsed.width())<.1);
                QCOMPARE(f.shell->facade()->current().id,current);QCOMPARE(f.shell->facade()->queue().items,queue);
            }
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void playlistMenusAreCompactAndDeletionNeedsConfirmation() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->libraryBridge();
        const auto key=bridge->createPlaylist("待确认删除的歌单");QVERIFY(bridge->addTracksToPlaylist(key,{QString::number(f.a.id),QString::number(f.b.id)}));QVERIFY(bridge->openPlaylist(key));QTest::qWait(320);
        const auto queue=f.shell->facade()->queue().items;
        QVERIFY(click(w,visual(root,"sidebarPlaylistOptions")));QTest::qWait(270);
        auto *sidebarMenu=w->findChild<QObject *>("playlistNavigationOptions");QVERIFY(sidebarMenu);QCOMPARE(sidebarMenu->property("width").toInt(),144);shot(w,"playlist-navigation-menu-144");
        QVERIFY(QMetaObject::invokeMethod(sidebarMenu,"close"));QTest::qWait(190);
        QVERIFY(click(w,visual(root,"playlistOptions")));QTest::qWait(270);
        auto *menu=w->findChild<QObject *>("playlistOptionsMenu");QVERIFY(menu);QCOMPARE(menu->property("width").toInt(),192);
        auto *entry=visual(root,"deletePlaylistEntry");QVERIFY(entry);QVERIFY(entry->property("destructive").toBool());shot(w,"playlist-options-delete");
        QVERIFY(click(w,entry));QTest::qWait(270);
        auto *confirmation=w->findChild<QObject *>("playlistDeletionConfirmation");QVERIFY(confirmation);QVERIFY(confirmation->property("opened").toBool());
        QCOMPARE(confirmation->property("background").value<QObject *>()->property("radius").toInt(),12);shot(w,"playlist-delete-confirmation");
        auto *footer=confirmation->property("footer").value<QQuickItem *>();QVERIFY(footer);QVERIFY(click(w,visual(footer,"confirmationButton1")));QTest::qWait(240);QCOMPARE(f.library.playlists().size(),1);
        QVERIFY(click(w,visual(root,"playlistOptions")));QTest::qWait(260);QVERIFY(click(w,visual(root,"playlistMarkerEntry")));QTest::qWait(280);
        auto *editor=w->findChild<QObject *>("playlistEditor");QVERIFY(editor);QVERIFY(editor->property("opened").toBool());
        for(auto *object:editor->findChildren<QObject *>())QVERIFY(!object->property("text").toString().startsWith("删除此歌单"));
        QVERIFY(QMetaObject::invokeMethod(editor,"reject"));QTest::qWait(230);
        QVERIFY(click(w,visual(root,"playlistOptions")));QTest::qWait(260);QVERIFY(click(w,visual(root,"deletePlaylistEntry")));QTest::qWait(260);
        footer=confirmation->property("footer").value<QQuickItem *>();QVERIFY(click(w,visual(footer,"confirmationButton0")));QTRY_COMPARE(f.library.playlists().size(),0);
        QCOMPARE(f.library.track(f.a.id).id,f.a.id);QCOMPARE(f.library.track(f.b.id).id,f.b.id);QCOMPARE(f.shell->facade()->queue().items,queue);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void lyricModeDoesNotJumpAndUsesTheAppearanceColorChooser() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *settings=f.shell->settingsBridge();QVERIFY(settings->setPreference("lyricColor",""));
        QVERIFY(f.shell->appBridge()->navigate(3));QTest::qWait(320);auto *dialog=w->findChild<QObject *>("lyricColorDialog");QVERIFY(dialog);
        auto chooser=[]()->QColorDialog *{for(auto *widget:QApplication::topLevelWidgets())if(widget->objectName()=="lyricDraftColorDialog")return qobject_cast<QColorDialog *>(widget);return nullptr;};
        for(int opening=0;opening<2;++opening) {
            QVERIFY(click(w,visual(root,"lyricColorSetting")));
            for(int frame=0;frame<20;++frame){QTest::qWait(16);QVERIFY(!dialog->property("custom").toBool());QVERIFY(visual(root,"lyricColorFollow")->property("checked").toBool());}
            QVERIFY(!visual(root,"chooseLyricDraftColor")->isEnabled());QVERIFY(!chooser());
            if(!opening){QVERIFY(click(w,visual(root,"cancelLyricColor")));QTest::qWait(240);}
        }
        shot(w,"lyric-follow-stable");QVERIFY(click(w,visual(root,"lyricColorCustom")));QVERIFY(click(w,visual(root,"chooseLyricDraftColor")));QTRY_VERIFY(chooser());QPointer<QColorDialog> palette=chooser();
        QVERIFY(palette->testOption(QColorDialog::DontUseNativeDialog));QVERIFY(palette->findChildren<QSpinBox *>().size()>=6);QVERIFY(!palette->findChildren<QLineEdit *>().isEmpty());
        const auto folder=qEnvironmentVariable("RELAXER_ADAPTIVE_SCREENSHOTS");if(!folder.isEmpty())palette->grab().save(QDir(folder).filePath("lyric-appearance-palette.png"));
        const auto before=dialog->property("chosenColor").value<QColor>();palette->setCurrentColor(QColor("#123456"));palette->reject();QTRY_VERIFY(palette.isNull());QCOMPARE(dialog->property("chosenColor").value<QColor>(),before);
        QVERIFY(click(w,visual(root,"chooseLyricDraftColor")));QTRY_VERIFY(chooser());palette=chooser();palette->setCurrentColor(QColor("#123456"));palette->accept();QTRY_VERIFY(palette.isNull());
        QCOMPARE(dialog->property("chosenColor").value<QColor>(),QColor("#123456"));QVERIFY(settings->values().value("lyricColor").toString().isEmpty());
        QVERIFY(click(w,visual(root,"saveLyricColor")));QTRY_COMPARE(settings->values().value("lyricColor").toString(),QString("#123456"));QTest::qWait(240);
        QVERIFY(click(w,visual(root,"lyricColorSetting")));QVERIFY(dialog->property("custom").toBool());
        for(int frame=0;frame<20;++frame){QTest::qWait(16);QVERIFY(dialog->property("custom").toBool());QVERIFY(visual(root,"lyricColorCustom")->property("checked").toBool());}
        QVERIFY(click(w,visual(root,"lyricColorFollow")));QVERIFY(click(w,visual(root,"saveLyricColor")));QTRY_VERIFY(settings->values().value("lyricColor").toString().isEmpty());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void playlistOrderIsDraggedInPlaceAndOnlySavedOnConfirmation() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->libraryBridge();
        const auto first=bridge->createPlaylist("第一歌单"),second=bridge->createPlaylist("第二歌单"),third=bridge->createPlaylist("第三歌单");
        QTRY_VERIFY(visual(root,"sidebarPlaylist-"+third));QTest::qWait(180);
        const auto track=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,rect(visual(root,"sidebarPlaylist-"+first)).center().toPoint());QTest::qWait(180);QVERIFY(!w->findChild<QObject *>("playlistEditor"));
        auto begin=[&](bool narrow) {
            if(!click(w,visual(root,narrow?"compactPlaylistOptions":"sidebarPlaylistOptions")))return false;
            QTest::qWait(260);return click(w,visual(root,"adjustPlaylistOrder"));
        };
        auto drag=[&](const QString &key,const QString &target,bool narrow) {
            const auto prefix=narrow?"compactSidebarPlaylist-":"sidebarPlaylist-";
            auto *source=visual(root,prefix+key),*destination=visual(root,prefix+target);if(!source||!destination)return false;
            const auto start=(rect(source).topLeft()+QPointF(11,18)).toPoint(),end=(rect(destination).topLeft()+QPointF(11,18)).toPoint();
            QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);
            for(int step=1;step<=12;++step){QTest::mouseMove(w,start+(end-start)*step/12,25);QTest::qWait(20);}
            QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,end);QTest::qWait(220);return true;
        };
        QVERIFY(begin(false));QTest::qWait(100);QVERIFY(w->property("playlistOrderEditing").toBool());QVERIFY(!visual(root,"sidebarPlaylistOptions"));
        auto *rotation=visual(root,"sidebarCreateRotation");QVERIFY(rotation);QVERIFY(rotation->rotation()>0&&rotation->rotation()<45);
        for(const auto &key:{first,second,third}){auto *handle=visual(root,"playlistOrderHandle-"+key);QVERIFY(handle);QVERIFY(handle->x()>-22&&handle->x()<0);}
        shot(w,"playlist-handles-entering");QTRY_COMPARE(rotation->rotation(),45.);QTRY_COMPARE(visual(root,"sidebarPlaylist-"+first)->property("sortingReveal").toReal(),1.);
        QVERIFY(drag(first,third,false));QTRY_COMPARE(visual(root,"sidebarPlaylist-"+first)->property("index").toInt(),2);
        QVERIFY(f.library.setting("qml.playlistOrder").isUndefined()||f.library.setting("qml.playlistOrder").isNull());shot(w,"playlist-order-editing");
        QVERIFY(click(w,visual(root,"cancelPlaylistOrder")));QTest::qWait(100);QVERIFY(w->property("playlistOrderDraftActive").toBool());QVERIFY(rotation->rotation()>0&&rotation->rotation()<45);
        QVERIFY(visual(root,"playlistOrderHandle-"+first)->x()<0);shot(w,"playlist-handles-leaving");QTRY_VERIFY(!w->property("playlistOrderDraftActive").toBool());QCOMPARE(rotation->rotation(),0.);QCOMPARE(visual(root,"sidebarPlaylist-"+first)->property("index").toInt(),0);
        QVERIFY(begin(false));QTest::qWait(370);QVERIFY(drag(third,first,false));QCOMPARE(visual(root,"sidebarPlaylist-"+third)->property("index").toInt(),0);
        QVERIFY(click(w,visual(root,"savePlaylistOrder")));QTRY_VERIFY(!w->property("playlistOrderEditing").toBool());
        QTRY_COMPARE(f.library.setting("qml.playlistOrder").toArray().first().toString(),third);
        {Library reopened(f.library.root());QVERIFY(reopened.open());PlaylistListModel restored(&reopened);QCOMPARE(restored.get(0).value("playlistKey").toString(),third);}
        w->resize(414,790);QTest::qWait(430);QVERIFY(click(w,visual(root,"compactNavigation")));QTest::qWait(310);
        QVERIFY(begin(true));QTest::qWait(370);QVERIFY(w->property("playlistOrderEditing").toBool());QVERIFY(drag(third,second,true));
        QVERIFY(click(w,visual(root,"compactCancelPlaylistOrder")));QTest::qWait(200);
        QCOMPARE(f.library.setting("qml.playlistOrder").toArray().first().toString(),third);shot(w,"playlist-order-narrow");
        QCOMPARE(f.shell->facade()->current().id,track);QCOMPARE(f.shell->facade()->queue().items,queue);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void sidebarCaptionsAnimateAndFooterMarginsMatch() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        auto *caption=visual(visual(root,"navigation0"),"navigationCaption");QVERIFY(caption);QCOMPARE(caption->opacity(),1.);
        QVERIFY(click(w,visual(root,"sidebarLogo")));QTest::qWait(60);w->grabWindow();
        QVERIFY(caption->opacity()>0&&caption->opacity()<1);QVERIFY(w->property("sidebarBaseWidth").toReal()>84&&w->property("sidebarBaseWidth").toReal()<205);shot(w,"sidebar-mid-collapse");
        QTest::qWait(300);QCOMPARE(caption->opacity(),0.);QVERIFY(click(w,visual(root,"sidebarLogo")));QTest::qWait(60);w->grabWindow();QVERIFY(caption->opacity()>0&&caption->opacity()<1);QTest::qWait(300);
        for(int width:{1240,900,547,546,414}) {
            w->resize(width,790);QTest::qWait(400);auto *bar=visual(root,"playerBar");const auto cover=rect(visual(root,"bottomCover")),barBox=rect(bar);
            QCOMPARE(cover.left(),qreal(11));QCOMPARE(cover.top()-barBox.top(),qreal(11));QCOMPARE(barBox.bottom()-cover.bottom(),qreal(11));QCOMPARE(cover.width(),qreal(76));
            if(width>=547){auto *queue=visual(root,"nextQueuePanel");const auto queueBox=rect(queue);QVERIFY(qAbs(w->width()-queueBox.right()-11)<.1);QVERIFY(qAbs(w->height()-queueBox.bottom()-11)<.1);QVERIFY(qAbs(rect(visual(queue,"upcomingToggle")).right()-queueBox.right())<.1);}
            shot(w,QString("footer-equal-margins-%1").arg(width));
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void lyricColorPopupAndFavoriteColumnsSaveTheirChoices() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *settings=f.shell->settingsBridge();
        auto *table=visual(root,"libraryListContainer");QVERIFY(table);auto *favorite=visual(table,"trackFavorite-"+QString::number(f.a.id));QVERIFY(favorite);QVERIFY(favorite->isVisible());
        const auto titleStart=rect(visual(table,"trackHeading-2")).left();
        QVERIFY(settings->values().value("showFavorites").toBool());QVERIFY(f.shell->appBridge()->navigate(3));QTest::qWait(320);
        auto *directory=visual(root,"openDataRoot"),*path=visual(root,"dataRootText");QVERIFY(directory);QVERIFY(path);
        QCOMPARE(directory->property("text").toString(),QString("打开本地音乐目录"));QVERIFY(qAbs(rect(directory).center().y()-rect(path).center().y())<1);
        auto *row=visual(root,"dataRootRow");QVERIFY(qAbs(rect(directory).right()-rect(row).right())<1);
        QVERIFY(click(w,visual(root,"lyricColorSetting")));QTest::qWait(280);auto *dialog=w->findChild<QObject *>("lyricColorDialog");QVERIFY(dialog);QVERIFY(dialog->property("opened").toBool());
        auto *picker=visual(root,"lyricColorPicker");QVERIFY(picker);QVERIFY(!picker->isEnabled());shot(w,"lyric-color-follow");
        QVERIFY(click(w,visual(root,"lyricColorFollow")));QVERIFY(click(w,visual(root,"lyricColorFollow")));QVERIFY(!picker->isEnabled());
        QVERIFY(click(w,visual(root,"lyricColorCustom")));QVERIFY(picker->isEnabled());QVERIFY(visual(root,"lyricColorCustom")->property("checked").toBool());QVERIFY(!visual(root,"lyricColorFollow")->property("checked").toBool());QVERIFY(QMetaObject::invokeMethod(dialog,"selectColor",Q_ARG(QVariant,QString("#123456"))));QTest::qWait(300);shot(w,"lyric-color-custom");
        w->resize(414,540);QTest::qWait(420);QVERIFY(rect(visual(root,"saveLyricColor")).bottom()<w->height());QVERIFY(rect(visual(root,"lyricColorFollow")).top()>0);shot(w,"lyric-color-414-540");
        QVERIFY(click(w,visual(root,"cancelLyricColor")));QTest::qWait(230);QVERIFY(settings->values().value("lyricColor").toString().isEmpty());w->resize(1240,790);QTest::qWait(420);
        QVERIFY(click(w,visual(root,"lyricColorSetting")));QTest::qWait(260);QVERIFY(click(w,visual(root,"lyricColorCustom")));QVERIFY(QMetaObject::invokeMethod(dialog,"selectColor",Q_ARG(QVariant,QString("#123456"))));QVERIFY(click(w,visual(root,"saveLyricColor")));
        QTRY_COMPARE(settings->values().value("lyricColor").toString(),QString("#123456"));QTest::qWait(240);
        QVERIFY(click(w,visual(root,"showFavoritesSetting")));QVERIFY(!settings->values().value("showFavorites").toBool());
        QVERIFY(f.shell->appBridge()->navigate(0));QTest::qWait(280);QVERIFY(!visual(table,"trackFavorite-"+QString::number(f.a.id))->isVisible());QCOMPARE(rect(visual(table,"trackHeading-2")).left(),titleStart-32);
        for(const QString id:{QString("-1"),QString("-2"),QString("0")}) {QVERIFY(f.shell->libraryBridge()->openPlaylist(id));QTest::qWait(120);QCOMPARE(visual(table,"trackHeading-1")->width(),0.);}
        w->resize(414,790);QTest::qWait(400);QCOMPARE(visual(table,"trackHeading-1")->width(),0.);
        QVERIFY(f.shell->appBridge()->navigate(3));QTest::qWait(320);QVERIFY(visual(root,"dataRootRow")->property("columns").toInt()==1);QVERIFY(rect(visual(root,"openDataRoot")).top()>rect(visual(root,"dataRootText")).bottom());shot(w,"settings-directory-narrow");
        QVERIFY(click(w,visual(root,"showFavoritesSetting")));QVERIFY(settings->values().value("showFavorites").toBool());QCOMPARE(visual(table,"trackHeading-1")->width(),32.);
        QVERIFY(click(w,visual(root,"lyricColorSetting")));QTest::qWait(260);QVERIFY(click(w,visual(root,"lyricColorFollow")));QVERIFY(!picker->isEnabled());QVERIFY(click(w,visual(root,"saveLyricColor")));QTRY_VERIFY(settings->values().value("lyricColor").toString().isEmpty());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void navigationFitsItsRowsAndKeepsTheSameVerticalPositions() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        w->resize(414,790);QTest::qWait(420);QVERIFY(click(w,visual(root,"compactNavigation")));QTest::qWait(310);
        auto *emptyList=visual(root,"compactPlaylistNavigation");QVERIFY(emptyList);QCOMPARE(emptyList->property("count").toInt(),0);QCOMPARE(emptyList->height(),0.);
        auto *emptyPopup=w->findChild<QObject *>("compactNavigationPanel");QVERIFY(emptyPopup);QVERIFY(emptyPopup->property("height").toInt()<390);shot(w,"navigation-empty");
        QVERIFY(QMetaObject::invokeMethod(emptyPopup,"close"));QTest::qWait(250);w->resize(1240,790);QTest::qWait(420);
        const auto key=f.shell->libraryBridge()->createPlaylist("侧栏高度");QVERIFY(key!="0");
        auto *playlist=visual(root,"sidebarPlaylist-"+key);QTRY_VERIFY((playlist=visual(root,"sidebarPlaylist-"+key)));
        QTest::qWait(160);
        QList<qreal> positions;
        for(int i=0;i<5;++i)positions.append(rect(visual(root,"navigation"+QString::number(i))).top());
        const auto playlistY=rect(playlist).top();
        for(int pass=0;pass<2;++pass) {
            QVERIFY(click(w,visual(root,"sidebarLogo")));QTest::qWait(340);
            for(int i=0;i<5;++i)QCOMPARE(rect(visual(root,"navigation"+QString::number(i))).top(),positions[i]);
            QCOMPARE(rect(playlist).top(),playlistY);
        }
        w->resize(414,790);QTest::qWait(420);QVERIFY(click(w,visual(root,"compactNavigation")));QTest::qWait(310);
        auto *popup=w->findChild<QObject *>("compactNavigationPanel");QVERIFY(popup);
        auto *list=visual(root,"compactPlaylistNavigation");QVERIFY(list);
        QTRY_VERIFY(list->property("count").toInt()>0);
        QCOMPARE(list->height(),list->property("contentHeight").toReal());
        const auto shortHeight=popup->property("height").toReal();QVERIFY(shortHeight<500);
        shot(w,"navigation-content-height");
        for(int i=0;i<14;++i)QVERIFY(f.shell->libraryBridge()->createPlaylist("列表 "+QString::number(i))!="0");
        QTRY_VERIFY(list->property("contentHeight").toReal()>list->height());
        QTRY_COMPARE(popup->property("height").toReal(),qreal(w->height()-98-52));
        QVERIFY(popup->property("height").toReal()>shortHeight);shot(w,"navigation-scrolling-height");
        w->resize(414,540);QTest::qWait(400);QVERIFY(rect(list).bottom()<=w->height()-98);
        QVERIFY(QMetaObject::invokeMethod(list,"positionViewAtEnd"));QTest::qWait(100);
        QVERIFY(list->property("atYEnd").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void editingMenusAndDialogControlsAreChineseAndRounded() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        auto *clipboard=QGuiApplication::clipboard();auto clipboardBackup=std::make_unique<QMimeData>();
        if(const auto *original=clipboard->mimeData())for(const auto &format:original->formats())clipboardBackup->setData(format,original->data(format));
        const auto restoreClipboard=qScopeGuard([&]{clipboard->setMimeData(clipboardBackup.release());});
        auto *dialog=w->findChild<QObject *>("bulkNewPlaylistDialog");QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));QTest::qWait(260);
        auto *footer=dialog->property("footer").value<QQuickItem *>();QVERIFY(footer);
        auto *ok=visual(footer,"confirmationButton0"),*cancel=visual(footer,"confirmationButton1");QVERIFY(ok);QVERIFY(cancel);
        QCOMPARE(ok->property("text").toString(),QString("确定"));QCOMPARE(cancel->property("text").toString(),QString("取消"));
        auto *field=visual(root,"bulkNewPlaylistName");QVERIFY(field);
        field->setProperty("text","中文编辑");field->forceActiveFocus();QVERIFY(QMetaObject::invokeMethod(field,"selectAll"));
        QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,rect(field).center().toPoint());QTest::qWait(280);
        auto *menu=field->findChild<QObject *>("textEditingMenu");QVERIFY(menu);QVERIFY(menu->property("opened").toBool());
        QCOMPARE(menu->property("background").value<QObject *>()->property("radius").toInt(),10);
        auto *content=menu->property("contentItem").value<QQuickItem *>();QVERIFY(content);
        std::function<QQuickItem *(QQuickItem *,const QString &)> findText=[&](QQuickItem *item,const QString &text)->QQuickItem * {
            if(item->inherits("QQuickMenuItem")&&item->property("text").toString()==text)return item;
            for(auto *child:item->childItems())if(auto *found=findText(child,text))return found;return nullptr;
        };
        for(const auto &text:QStringList{"撤销","重做","剪切","复制","粘贴","删除","全选"})QVERIFY2(findText(content,text),qPrintable(text));
        QVERIFY(click(w,findText(content,"复制")));QCOMPARE(QGuiApplication::clipboard()->text(),QString("中文编辑"));
        QTest::qWait(200);QVERIFY(click(w,cancel));QTest::qWait(200);
        const auto key=f.shell->libraryBridge()->createPlaylist("信息编辑");QVERIFY(f.shell->libraryBridge()->openPlaylist(key));QTest::qWait(320);
        QVERIFY(QMetaObject::invokeMethod(visual(root,"localPage"),"editMetadata"));QTest::qWait(260);
        auto *description=visual(root,"playlistDescriptionEdit");QVERIFY(description);
        QCOMPARE(description->property("background").value<QObject *>()->property("radius").toInt(),8);shot(w,"metadata-chinese-rounded");
        QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,rect(description).center().toPoint());QTest::qWait(280);
        auto *areaMenu=description->findChild<QObject *>("textEditingMenu");QVERIFY(areaMenu);QVERIFY(areaMenu->property("opened").toBool());
        QVERIFY(!areaMenu->property("protectedText").toBool());QVERIFY(QMetaObject::invokeMethod(areaMenu,"close"));QTest::qWait(190);
        auto *metadata=w->findChild<QObject *>("playlistMetadataDialog");QVERIFY(metadata);QVERIFY(QMetaObject::invokeMethod(metadata,"reject"));QTest::qWait(220);
        QVERIFY(click(w,visual(root,"sidebarLogo")));QTest::qWait(300);
        auto *nav=visual(root,"navigation0");QVERIFY(nav);QTest::mouseMove(w,rect(nav).center().toPoint());QTest::qWait(880);
        auto *hint=nav->findChild<QObject *>("meaningfulHint");QVERIFY(hint);QVERIFY(hint->property("visible").toBool());
        QCOMPARE(hint->property("background").value<QObject *>()->property("radius").toInt(),8);shot(w,"navigation-rounded-tooltip");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void queueOpensFromItsButtonAndUsesOneGlassSurface() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();w->resize(414,790);QTest::qWait(420);
        for(bool expanded:{false,true}) {
            if(expanded){QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(500);}
            auto *button=visual(root,"compactQueueButton");QVERIFY(click(w,button));
            auto *popup=w->findChild<QObject *>("narrowQueuePopup");QVERIFY(popup);auto *surface=visual(root,"narrowQueueSurface");QVERIFY(surface);
            const auto anchor=rect(button).center();
            const QRectF target(rect(button).topLeft()+QPointF(popup->property("x").toReal(),popup->property("y").toReal()),QSizeF(popup->property("width").toReal(),popup->property("height").toReal()));
            bool moving=false;
            for(int frame=0;frame<22;++frame) {
                QTest::qWait(16);w->grabWindow();const auto progress=popup->property("reveal").toReal();
                if(progress>0&&progress<1)moving=true;
                const auto actual=rect(surface);const auto expected=anchor+(target.topLeft()-anchor)*progress;
                QVERIFY2(QLineF(actual.topLeft(),expected).length()<1,"Queue does not grow from its button");QVERIFY(qAbs(actual.width()-target.width()*progress)<1);
                if(frame==5)shot(w,expanded?"queue-origin-expanded":"queue-origin-normal");
            }
            QVERIFY(moving);QVERIFY(visual(root,"queueGlassSurface"));QVERIFY(popup->property("background").value<QObject *>()==nullptr);
            if(qEnvironmentVariable("QT_QUICK_BACKEND")!="software"){QTRY_VERIFY(visual(root,"queueBackdropBlur"));QVERIFY(visual(root,"queueBackdropBlur")->property("blurEnabled").toBool());}
            shot(w,expanded?"queue-glass-expanded":"queue-glass-normal");
            QVERIFY(click(w,button));QTest::qWait(240);QVERIFY(!popup->property("visible").toBool());
        }
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTest::qWait(480);QVERIFY(click(w,visual(root,"compactNavigation")));QTest::qWait(310);
        auto *navigation=w->findChild<QObject *>("compactNavigationPanel");QVERIFY(navigation);QCOMPARE(navigation->property("width").toInt(),168);shot(w,"navigation-168");
        QVERIFY(QMetaObject::invokeMethod(navigation,"close"));QTest::qWait(250);
        QVERIFY(click(w,visual(root,"librarySearchButton")));QTest::qWait(300);auto *glass=visual(root,"searchGlassSurface");QVERIFY(glass);shot(w,"search-single-border");
        int rectangles=0;for(auto *item:glass->childItems())if(item->inherits("QQuickRectangle"))++rectangles;QCOMPARE(rectangles,1);
        if(qEnvironmentVariable("QT_QUICK_BACKEND")!="software")QTRY_VERIFY(visual(root,"searchBackdropBlur"));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void settingsControlsAndRegionalFontTableRemainUsable() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *settings=f.shell->settingsBridge();QVERIFY(f.shell->appBridge()->navigate(3));QTest::qWait(320);
        QVERIFY(visual(root,"thresholdFrame"));const auto ratio=settings->values().value("threshold").toInt();
        QVERIFY(click(w,visual(root,"thresholdIncrease")));QTRY_COMPARE(settings->values().value("threshold").toInt(),ratio+1);
        QVERIFY(click(w,visual(root,"thresholdDecrease")));QTRY_COMPARE(settings->values().value("threshold").toInt(),ratio);
        auto findColor=[]()->QColorDialog *{for(auto *widget:QApplication::topLevelWidgets())if(widget->objectName()=="lyricFullColorDialog")return qobject_cast<QColorDialog *>(widget);return nullptr;};
        for(int press=0;press<2;++press){QVERIFY(click(w,visual(root,"lyricColorSetting")));QTest::qWait(250);QVERIFY(!findColor());QVERIFY(click(w,visual(root,"cancelLyricColor")));QTest::qWait(220);}
        QVERIFY(settings->values().value("lyricColor").toString().isEmpty());
        QVERIFY(settings->chooseLyricColor(w));QTRY_VERIFY(findColor());QPointer<QColorDialog> chooser=findColor();
        QVERIFY(chooser->testOption(QColorDialog::DontUseNativeDialog));QVERIFY(chooser->findChildren<QSpinBox *>().size()>=6);QVERIFY(!chooser->findChildren<QLineEdit *>().isEmpty());
        for(auto *label:chooser->findChildren<QLabel *>())QVERIFY2(!label->text().contains("colors")&&!label->text().contains("Hue")&&!label->text().contains("HTML"),qPrintable(label->text()));
        auto *colorButtons=chooser->findChild<QDialogButtonBox *>();QVERIFY(colorButtons);
        QCOMPARE(colorButtons->button(QDialogButtonBox::Ok)->text(),QString("确定"));QCOMPARE(colorButtons->button(QDialogButtonBox::Cancel)->text(),QString("取消"));
        QVERIFY(chooser->styleSheet().contains("border-radius:8px"));
        int roundedSurfaces=0;
        for(auto *widget:chooser->findChildren<QWidget *>())if(!widget->mask().isEmpty())++roundedSurfaces;
        QCOMPARE(roundedSurfaces,3);QCOMPARE(chooser->findChildren<QWidget *>("roundedColorSwatches").size(),2);
        const auto firstSpin=chooser->findChildren<QSpinBox *>().first();const auto spinValue=firstSpin->value();
        firstSpin->stepUp();QCOMPARE(firstSpin->value(),qMin(firstSpin->maximum(),spinValue+firstSpin->singleStep()));
        const auto folder=qEnvironmentVariable("RELAXER_ADAPTIVE_SCREENSHOTS");if(!folder.isEmpty())chooser->grab().save(QDir(folder).filePath("color-chinese-rounded.png"));
        chooser->setCurrentColor(QColor("#123456"));chooser->reject();QTRY_VERIFY(chooser.isNull());QVERIFY(settings->values().value("lyricColor").toString().isEmpty());
        QVERIFY(settings->chooseLyricColor(w));QTRY_VERIFY(findColor());chooser=findColor();chooser->setCurrentColor(QColor("#123456"));chooser->accept();QTRY_COMPARE(settings->values().value("lyricColor").toString(),QString("#123456"));QTRY_VERIFY(chooser.isNull());
        QVERIFY(click(w,visual(root,"uiFontSetting")));QTest::qWait(300);auto *dialog=w->findChild<QObject *>("uiFontDialog");QVERIFY(dialog);QVERIFY(dialog->property("opened").toBool());
        const auto before=settings->fontFiles();
        const auto font=QUrl::fromLocalFile(qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/segoeui.ttf").toString();
        QVERIFY(QMetaObject::invokeMethod(dialog,"setFile",Q_ARG(QVariant,QString("songs")),Q_ARG(QVariant,font)));QCOMPARE(settings->fontFiles(),before);shot(w,"font-table-wide");
        w->resize(414,540);QTest::qWait(420);
        for(const auto &region:QStringList{"songs","lyrics","footer","next","rest"}) {
            auto *choose=visual(root,"chooseFont-"+region),*reset=visual(root,"resetFont-"+region);QVERIFY(choose);QVERIFY(reset);QVERIFY(rect(choose).right()<rect(reset).left());QVERIFY(rect(reset).right()<=w->width()-16);
        }
        shot(w,"font-table-414");QVERIFY(QMetaObject::invokeMethod(dialog,"close"));QTest::qWait(230);QCOMPARE(settings->fontFiles(),before);
        QVERIFY(QMetaObject::invokeMethod(dialog,"openSettings"));QTest::qWait(270);QCOMPARE(visual(root,"fontRegionFile-songs")->property("text").toString(),QString("系统字体"));
        QVERIFY(QMetaObject::invokeMethod(dialog,"setFile",Q_ARG(QVariant,QString("songs")),Q_ARG(QVariant,font)));
        QVERIFY(click(w,visual(root,"saveRegionFonts")));QTest::qWait(250);QVERIFY(!dialog->property("visible").toBool());QVERIFY(!settings->fontFamilyFor("songs").isEmpty());QVERIFY(settings->fontFamilyFor("lyrics").isEmpty());
        QVERIFY(QMetaObject::invokeMethod(dialog,"openSettings"));QTest::qWait(270);QCOMPARE(visual(root,"fontRegionFile-songs")->property("text").toString(),QString("segoeui.ttf"));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void settingsRowsWrapAndRestoreAfterNarrowMode() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();QVERIFY(f.shell->appBridge()->navigate(3));QTest::qWait(300);
        const QStringList cells{"settingsAppearanceCell","settingsThresholdCell","settingsCloseCell"};
        for(int width:{1240,1000,900,600,546,414,547,1000,1240}) {
            w->resize(width,790);QTest::qWait(380);w->grabWindow();
            auto *flow=visual(root,"settingsBasicGrid");QVERIFY(flow);
            QList<QRectF> boxes;
            for(const auto &name:cells) {
                auto *cell=visual(root,name);QVERIFY(cell);const auto box=rect(cell);boxes.append(box);
                QVERIFY2(box.left()>=rect(flow).left()-.5&&box.right()<=rect(flow).right()+.5,qPrintable(name));
            }
            for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)QVERIFY(!boxes[i].intersects(boxes[j]));
            if(width<547) {
                QVERIFY(boxes[1].top()>boxes[0].bottom());QVERIFY(boxes[2].top()>boxes[1].bottom());
                for(const auto &box:boxes)QVERIFY(qAbs(box.width()-flow->width())<1);
            }else if(width==1240) {
                QVERIFY(qAbs(boxes[0].top()-boxes[1].top())<1);QVERIFY(qAbs(boxes[1].top()-boxes[2].top())<1);
            }else if(width==900||width==1000) {
                QVERIFY(qAbs(boxes[0].top()-boxes[1].top())<1);QVERIFY(boxes[2].top()>boxes[0].bottom());
            }
            if(width==414||width==900||width==1240)shot(w,QString("settings-wrapping-%1").arg(width));
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void playlistCoverIsPassiveAndMarkerDraftsAreSaved() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bridge=f.shell->libraryBridge();
        const auto key=bridge->createPlaylist("夜间音乐"),second=bridge->createPlaylist("晨间音乐");QVERIFY(key!="0");
        QVERIFY(bridge->addTracksToPlaylist(key,{QString::number(f.a.id)}));QVERIFY(bridge->openPlaylist(key));QTest::qWait(350);
        auto *picker=w->findChild<QObject *>("playlistCoverPicker");QVERIFY(picker);QSignalSpy coverOpened(picker,SIGNAL(visibleChanged()));QVERIFY(coverOpened.isValid());
        QVERIFY(click(w,visual(root,"playlistCover")));QTest::qWait(180);QVERIFY(!picker->property("visible").toBool());QCOMPARE(coverOpened.count(),0);
        QVERIFY(click(w,visual(root,"playlistOptions")));QTest::qWait(260);QVERIFY(visual(root,"playlistCoverEntry")->isEnabled());
        QVERIFY(click(w,visual(root,"playlistMarkerEntry")));QTest::qWait(280);
        auto *editor=w->findChild<QObject *>("playlistEditor");QVERIFY(editor);QVERIFY(editor->property("opened").toBool());QCOMPARE(bridge->playlistMarkerIcons().size(),16);
        relaxer::UiImages images;
        for(const auto &name:bridge->playlistMarkerIcons()) {
            QVERIFY(visual(root,"playlistMarker-"+name));
            const auto image=images.requestImage("icon/mark-"+name+"?color=%23e5455f",nullptr,{});
            int colored=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).alpha()>64)++colored;
            QVERIFY2(colored>40,qPrintable(name+" did not render"));
        }
        shot(w,"playlist-marker-icons");
        QVERIFY(click(w,visual(root,"playlistMarker-moon")));QVERIFY(QMetaObject::invokeMethod(editor,"reject"));QTest::qWait(230);QVERIFY(!bridge->playlistMarkers().contains(key));
        QVERIFY(QMetaObject::invokeMethod(editor,"edit",Q_ARG(QVariant,key),Q_ARG(QVariant,QString("夜间音乐"))));QTest::qWait(280);
        QVERIFY(click(w,visual(root,"playlistMarker-star")));
        QSignalSpy resets(f.shell->libraryModel()->sourceModel(),&QAbstractItemModel::modelReset);
        const auto track=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        QVERIFY(QMetaObject::invokeMethod(editor,"accept"));QTest::qWait(250);QCOMPARE(bridge->playlistMarkers().value(key).toString(),QString("icon:star"));QCOMPARE(resets.count(),0);
        QVERIFY(QMetaObject::invokeMethod(editor,"edit",Q_ARG(QVariant,second),Q_ARG(QVariant,QString("晨间音乐"))));QTest::qWait(280);
        QVERIFY(click(w,visual(root,"playlistTextMarkMode")));QTest::qWait(80);QVERIFY(visual(root,"playlistMarkerText")->setProperty("text","晨"));shot(w,"playlist-marker-text");
        QVERIFY(QMetaObject::invokeMethod(editor,"accept"));QTest::qWait(250);QCOMPARE(bridge->playlistMarkers().value(second).toString(),QString("text:晨"));
        QVERIFY(click(w,visual(root,"sidebarLogo")));QTest::qWait(350);
        QVERIFY(visual(root,"librarySectionDivider")->isVisible());QVERIFY(visual(root,"playlistSectionDivider")->isVisible());
        auto *first=visual(root,"sidebarPlaylist-"+key),*other=visual(root,"sidebarPlaylist-"+second);QVERIFY(first);QVERIFY(other);QVERIFY(first->isVisible());QVERIFY(other->isVisible());
        QCOMPARE(first->property("marker").toString(),QString("mark-star"));QCOMPARE(other->property("textMarker").toString(),QString("晨"));shot(w,"sidebar-playlist-markers");
        QVERIFY(click(w,other));QTest::qWait(400);QCOMPARE(bridge->playlistId(),second);QCOMPARE(f.shell->facade()->current().id,track);QCOMPARE(f.shell->facade()->queue().items,queue);
        w->resize(414,540);QTest::qWait(400);QVERIFY(QMetaObject::invokeMethod(editor,"edit",Q_ARG(QVariant,key),Q_ARG(QVariant,QString("夜间音乐"))));QTest::qWait(280);
        auto *dialogItem=qvariant_cast<QQuickItem *>(editor->property("background"));QVERIFY(dialogItem);QVERIFY(rect(dialogItem).top()>=0);QVERIFY(rect(dialogItem).bottom()<=w->height());shot(w,"playlist-marker-minimum");
        QVERIFY(QMetaObject::invokeMethod(editor,"reject"));QTest::qWait(230);w->resize(1240,790);QTest::qWait(380);
        QCOMPARE(qRound(visual(root,"sidebar")->width()),84);QVERIFY(other->isVisible());QCOMPARE(other->property("textMarker").toString(),QString("晨"));
        {Library reopened(f.library.root());QVERIFY(reopened.open());QCOMPARE(reopened.setting("qml.playlistMarkers").toObject().value(key).toString(),QString("icon:star"));QCOMPARE(reopened.setting("qml.playlistMarkers").toObject().value(second).toString(),QString("text:晨"));}
        QVERIFY(!bridge->setPlaylistMarker(key,"text:两个"));QVERIFY(!bridge->setPlaylistMarker(key,"icon:missing"));QVERIFY(!bridge->setPlaylistMarker("-1","icon:star"));
        QCOMPARE(bridge->playlistMarkers().value(key).toString(),QString("icon:star"));
        QVERIFY(bridge->deletePlaylist(key));QVERIFY(!bridge->playlistMarkers().contains(key));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void resizingDoesNotRestartTheCurrentArtworkTransition() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(520);
        auto *cover=visual(root,"currentArtwork");QVERIFY(cover);
        QTRY_COMPARE(cover->property("transition").toReal(),1.);
        const auto image=cover->property("displayedUrl").toUrl();QVERIFY(!image.isEmpty());
        QSignalSpy changes(cover,SIGNAL(displayedUrlChanged()));QVERIFY(changes.isValid());
        const auto current=f.shell->facade()->current().id;const auto queue=f.shell->facade()->queue().items;
        for(int pass=0;pass<2;++pass)for(int width:{1240,1100,970,820,700,600,548,546,460,414,546,700,970,1240}) {
            w->resize(width,790);QTest::qWait(20);w->grabWindow();
            QCOMPARE(cover->property("displayedUrl").toUrl(),image);
            QCOMPARE(cover->property("transition").toReal(),1.);
            QVERIFY(!cover->property("pendingTransition").toBool());
            QCOMPARE(f.shell->facade()->current().id,current);QCOMPARE(f.shell->facade()->queue().items,queue);
        }
        QCOMPARE(changes.count(),0);QTest::qWait(350);shot(w,"cover-after-resizing");
        f.shell->playbackBridge()->next();
        QTRY_VERIFY(cover->property("displayedUrl").toUrl()!=image);
        QTRY_VERIFY(cover->property("pendingTransition").toBool());
        QTRY_COMPARE(cover->property("transition").toReal(),1.);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void collapsingNarrowPlaybackKeepsMetadataOnSeparateRows() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        for(int width:{414,460,546}) {
            w->resize(width,790);QTest::qWait(420);
            QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(480);
            QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));
            for(int frame=0;frame<28;++frame) {
                QTest::qWait(16);w->grabWindow();
                auto *title=visual(root,"sharedTitleFlight"),*artist=visual(root,"sharedArtistFlight");QVERIFY(title);QVERIFY(artist);
                if(title->isVisible()&&artist->isVisible()) {
                    QVERIFY2(rect(title).bottom()+2<=rect(artist).top(),"Title and artist overlap while returning to the footer");
                    if(!visual(root,"currentTitle")->isVisible())QCOMPARE(title->opacity(),1-w->property("coverProgress").toReal());
                }
                if(frame==8)shot(w,QString("collapsing-metadata-%1").arg(width));
            }
            QVERIFY(!w->property("metadataTravelling").toBool());
            QVERIFY(rect(visual(root,"currentTitle")).bottom()+3<=rect(visual(root,"currentArtist")).top());
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void resizingPreservesPlaybackAndOptionsMoveTheList() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *player=f.shell->facade();
        // Keep layout and animation frames advancing when a native popup or
        // another app occludes the fixture window on the desktop.
        const auto renderFor=[&](int duration){for(int elapsed=0;elapsed<duration;elapsed+=16){w->grabWindow();QTest::qWait(qMin(16,duration-elapsed));}w->grabWindow();};
        const auto current=player->current().id;const auto queue=player->queue().items;
        QTRY_VERIFY(player->seekable());player->seek(4000);QTRY_VERIFY(player->position()>=4000);
        auto *table=visual(root,"libraryListContainer");QVERIFY(table);
        const auto widths=f.shell->settingsBridge()->values().value("songColumnWidths");
        for(const int width:{900,760,548,547,546,460,414,1240}) {
            const auto before=player->position();w->resize(width,790);renderFor(420);
            QCOMPARE(w->height(),790);QCOMPARE(w->property("narrowLayout").toBool(),width<547);
            QCOMPARE(player->current().id,current);QCOMPARE(player->queue().items,queue);QVERIFY(player->playing());QVERIFY(player->position()>=before-100);
            auto *side=visual(root,"sidebar");QVERIFY(side);
            QVERIFY2(QTest::qWaitFor([&]{w->grabWindow();return qRound(side->width())==(width<547?0:205);},2000),
                     qPrintable(QString("Sidebar did not settle at width %1; narrow progress %2").arg(width).arg(w->property("narrowProgress").toReal())));
            for(const auto *name:{"playAllButton","listPlayModeStart","currentFavoriteButton","playPauseButton","playMode","muteButton"}) {
                auto *item=QString::fromLatin1(name)=="listPlayModeStart"?visual(visual(root,"listPlayMode"),name):visual(root,name);QVERIFY2(item,name);const auto box=rect(item);
                QVERIFY2(box.left()>=0&&box.right()<=width+1,name);
            }
            shot(w,QString("library-%1").arg(width));
        }
        w->resize(460,790);renderFor(420);const auto top=rect(table).top();
        QVERIFY(click(w,visual(root,"moreLibraryOptions")));renderFor(90);
        QVERIFY(rect(table).top()>top);QVERIFY(rect(table).top()<top+46);
        renderFor(290);
        QVERIFY(rect(table).top()>top+40);QVERIFY(!visual(root,"findMissingLyrics")->isEnabled());
        shot(w,"library-options-460");
        QVERIFY(click(w,visual(root,"moreLibraryOptions")));renderFor(380);QVERIFY(qAbs(rect(table).top()-top)<1);
        QCOMPARE(f.shell->settingsBridge()->values().value("songColumnWidths"),widths);
        QVERIFY(f.shell->libraryBridge()->openPlaylist("-1"));QVERIFY(!visual(root,"findMissingLyrics")->isVisible());
        QVERIFY(click(w,visual(root,"moreLibraryOptions")));renderFor(350);
        QVERIFY(visual(root,"localPage")->property("optionsExpanded").toBool());
        QVERIFY(f.shell->appBridge()->navigate(1));renderFor(80);QVERIFY(f.shell->appBridge()->navigate(0));renderFor(350);
        QVERIFY(!visual(root,"localPage")->property("optionsExpanded").toBool());
        shot(w,"favorites-460");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void queueLyricsAndBulkFitTheNarrowWindow() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        w->resize(460,790);QTest::qWait(420);
        QVERIFY(rect(visual(root,"muteButton")).left()>rect(visual(root,"playMode")).right());
        QVERIFY(qAbs(rect(visual(root,"muteButton")).right()-(w->width()-18))<1);
        QTest::mouseMove(w,rect(visual(root,"muteButton")).center().toPoint());QCoreApplication::processEvents();
        auto *volume=w->findChild<QObject *>("compactVolumePopup");QVERIFY(volume);QVERIFY(volume->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(volume,"close"));QTest::qWait(180);
        QVERIFY(click(w,visual(root,"compactQueueButton")));QTest::qWait(360);
        auto *popup=w->findChild<QObject *>("narrowQueuePopup");QVERIFY(popup);QVERIFY(popup->property("opened").toBool());
        shot(w,"queue-460");QVERIFY(click(w,visual(root,"compactQueueButton")));QTest::qWait(250);QVERIFY(!popup->property("visible").toBool());
        QVERIFY(click(w,visual(root,"compactQueueButton")));QTest::qWait(320);QVERIFY(popup->property("opened").toBool());
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(300,400));QTest::qWait(250);QVERIFY(!popup->property("visible").toBool());
        const auto controlX=rect(visual(root,"playerCenter")).x();
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(100);
        const auto movingX=rect(visual(root,"playerCenter")).x();QVERIFY(movingX<controlX);QVERIFY(movingX>18);
        shot(w,"footer-centering-460");QTest::qWait(360);
        const auto art=rect(visual(root,"currentArtwork")),title=rect(visual(root,"currentTitle"));Q_UNUSED(title)
        auto *lyrics=visual(root,"lyricsPanel");QVERIFY(lyrics);QVERIFY(rect(lyrics).top()>art.bottom());
        QVERIFY(!visual(root,"compactNavigation")->isVisible());QVERIFY(rect(visual(root,"playerCollapse")).top()<2);
        auto *bar=visual(root,"playerBar"),*cover=visual(root,"bottomCover"),*seek=visual(root,"playbackProgress");QVERIFY(bar);QVERIFY(cover);QVERIFY(seek);
        QCOMPARE(qRound(bar->height()),98);QCOMPARE(qRound(cover->width()),76);QVERIFY(rect(seek).width()<300);
        QVERIFY(rect(visual(root,"currentFavoriteButton")).left()>rect(cover).right());
        QVERIFY(qAbs((rect(visual(root,"compactQueueButton")).left()+rect(visual(root,"muteButton")).right())/2-w->width()/2.)<1);
        QVERIFY(qAbs(rect(seek).center().x()-w->width()/2.)<1);
        QCOMPARE(qRound(art.width()),88);QCOMPARE(visual(root,"playerCollapse")->property("text").toString(),QString());
        shot(w,"lyrics-460");
        w->resize(1240,790);QTest::qWait(420);shot(w,"lyrics-wide-restored");
        w->resize(460,790);QTest::qWait(420);QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTest::qWait(460);
        w->resize(460,540);QTest::qWait(350);
        auto *table=visual(root,"libraryListContainer");QVERIFY(QMetaObject::invokeMethod(table,"beginBulk"));QTest::qWait(150);
        auto *bulk=visual(root,"bulkBar");QVERIFY(bulk);QVERIFY(bulk->isVisible());
        for(const char *name:{"bulkSelectAll","bulkClear","bulkLyrics","bulkInstrumental","bulkExit","bulkDelete","bulkLike","bulkNewPlaylist"}) {
            auto *item=visual(root,name);QVERIFY(item);QVERIFY2(rect(item).right()<=w->width()+1,name);QVERIFY2(rect(item).bottom()<=w->height()+1,name);
        }
        QVERIFY(rect(table).bottom()<=rect(bulk).top()+1);shot(w,"bulk-460");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void profileStartsCollapsedAndRequiresAnExplicitPull() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        QVERIFY(f.shell->appBridge()->navigate(2));QTest::qWait(250);
        auto *page=visual(root,"profilePage");QVERIFY(page);QVERIFY(!page->property("statisticsExpanded").toBool());
        wheel(w,QPoint(600,450),120);QTest::qWait(300);QVERIFY(page->property("statisticsExpanded").toBool());shot(w,"profile-wide-expanded");
        w->resize(460,790);QTest::qWait(420);QVERIFY(!page->property("statisticsExpanded").toBool());
        wheel(w,QPoint(300,450),120);QTest::qWait(250);QVERIFY(!page->property("statisticsExpanded").toBool());
        const QStringList labels{"歌曲数量：","累计循环：","累计时长："};
        for(int index=0;index<3;++index) {
            auto *cell=visual(root,"profileStatisticCell-"+QString::number(index));QVERIFY(cell);
            auto *caption=visual(root,"profileStatisticCaption-"+QString::number(index));QVERIFY(caption);QCOMPARE(caption->property("text").toString(),labels[index]);
            QVERIFY(qAbs(rect(cell).top()-rect(visual(root,"profileStatisticCell-0")).top())<1);
            if(index)QVERIFY(rect(cell).left()>rect(visual(root,"profileStatisticCell-"+QString::number(index-1))).right());
        }
        shot(w,"profile-460");
        w->resize(1240,790);QTest::qWait(420);QVERIFY(!page->property("statisticsExpanded").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void transitionsStayInsideAndNavigationSlidesFromTheLeft() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        for(bool expanded:{false,true}) {
            w->resize(900,790);QTest::qWait(420);
            if(expanded){QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(520);}
            for(int width:{546,547,460}) {
                w->resize(width,790);
                for(int frame=0;frame<25;++frame) {
                    QTest::qWait(16);
                    w->grabWindow(); // Verify geometry after the same polish pass that paints the frame.
                    for(const auto &name:expanded?QStringList{"currentArtwork","expandedSongTitle","expandedSongArtist","lyricsPanel"}:QStringList{"paperSurface","libraryListContainer","trackHeading-2","trackHeading-3"}) {
                        auto *item=visual(root,name);QVERIFY2(item,qPrintable(name));const auto box=rect(item);
                        QVERIFY2(box.left()>=-.5,qPrintable(name+" moved beyond the left edge"));
                        QVERIFY2(box.right()<=width+.5,qPrintable(name+QString(" right=%1 width=%2 frame=%3 progress=%4").arg(box.right()).arg(width).arg(frame).arg(w->property("narrowProgress").toReal())));
                    }
                    if(expanded) {
                        const auto cover=rect(visual(root,"currentArtwork")),title=rect(visual(root,"expandedSongTitle"));
                        QVERIFY2(!cover.intersects(title),"Cover overlaps title during adaptive animation");
                        auto *group=visual(root,"expandedMetadataGroup");auto *lyrics=visual(root,"lyricsList");
                        QVERIFY2(!rect(group).intersects(rect(lyrics)),"Metadata overlaps lyrics during adaptive animation");
                        auto *panel=visual(root,"nextQueuePanel");const auto box=rect(panel);
                        const auto progress=w->property("narrowProgress").toReal();
                        auto *bar=visual(root,"playerBar");const auto targetX=bar->x()+bar->property("queueControlX").toReal();
                        const auto targetY=bar->y()+bar->property("queueControlY").toReal();
                        QVERIFY(qAbs(box.x()-(8+(targetX-8)*progress))<1);
                        QVERIFY(qAbs(box.y()-((w->height()-4-panel->height())*(1-progress)+targetY*progress))<1);
                    }
                    if(frame==8)shot(w,QString("transition-%1-%2").arg(expanded?"lyrics":"library").arg(width));
                }
            }
        }
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTest::qWait(520);
        auto *nav=visual(root,"compactNavigation"),*close=visual(root,"windowControl2");QVERIFY(nav);QVERIFY(close);
        QCOMPARE(rect(nav).size(),rect(close).size());QVERIFY(qAbs(rect(nav).center().y()-rect(close).center().y())<1);
        QVERIFY(qAbs(rect(nav).center().x()-(w->width()-rect(close).center().x()))<1);
        QVERIFY(click(w,nav));QTest::qWait(80);
        auto *popup=w->findChild<QObject *>("compactNavigationPanel");QVERIFY(popup);
        auto *surface=qvariant_cast<QQuickItem *>(popup->property("contentItem"));QVERIFY(surface);
        const auto moving=rect(surface).left();QVERIFY(moving<22);QVERIFY(moving>-280);shot(w,"navigation-entering-460");
        QTest::qWait(260);QVERIFY(popup->property("opened").toBool());QVERIFY(qAbs(rect(surface).left()-22)<1);
        QVERIFY(!popup->property("modal").toBool());QVERIFY(!popup->property("dim").toBool());shot(w,"navigation-460");
        QVERIFY(click(w,visual(root,"compactNavigation")));QTest::qWait(270);QVERIFY(!popup->property("visible").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void metadataAndRightQueueRespectTheirAvailableSpace() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *bar=visual(root,"playerBar");QVERIFY(bar);
        for(const auto &sample:QList<QStringList>{{"abcdef","a…","false"},{"abcdef","ab…","false"},{"abcdef","abc…","true"},{"abcde","ab…","true"},{"abcde","a…","false"},{"ab","…","true"}}) {
            QVariant result;QVERIFY(QMetaObject::invokeMethod(bar,"showMetadata",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,sample[0]),Q_ARG(QVariant,sample[1])));
            QCOMPARE(result.toBool(),sample[2]=="true");
        }
        w->resize(460,790);QTest::qWait(420);QVERIFY(!visual(root,"currentTitle")->isVisible());
        w->resize(414,790);QTest::qWait(420);
        QVERIFY(rect(visual(root,"compactQueueButton")).left()>=rect(visual(root,"bottomCover")).right()+11);
        QVERIFY(qAbs(rect(visual(root,"muteButton")).right()-(w->width()-18))<1);shot(w,"minimum-414");
        w->resize(546,790);QTest::qWait(420);auto *title=visual(root,"currentTitle"),*artist=visual(root,"currentArtist");QVERIFY(title->isVisible());QVERIFY(artist->isVisible());
        const auto cover=rect(visual(root,"bottomCover")),controls=rect(visual(root,"compactQueueButton"));
        QVERIFY(rect(title).left()>cover.right());QVERIFY(rect(title).right()<controls.left());QVERIFY(rect(artist).right()<controls.left());shot(w,"metadata-546");
        for(int width:{900,970,1240}) {
            w->resize(width,790);QTest::qWait(420);w->grabWindow();auto *panel=visual(root,"nextQueuePanel");QVERIFY(panel);
            QTRY_VERIFY2(qAbs(rect(panel).bottom()-(w->height()-11))<1,qPrintable(QString("bottom=%1 expected=%2 narrow=%3 travel=%4").arg(rect(panel).bottom()).arg(w->height()-11).arg(w->property("narrowProgress").toReal()).arg(w->property("upcomingTravel").toReal())));QVERIFY(rect(panel).left()>rect(visual(root,"playerCenter")).right());
            QVERIFY(click(w,visual(panel,"upcomingToggle")));QTest::qWait(350);w->grabWindow();QVERIFY(panel->property("panelExpanded").toBool());
            if(qEnvironmentVariable("QT_QUICK_BACKEND")!="software")QTRY_VERIFY(visual(root,"wideQueueBackdropBlur"));
            auto *rowTitle=visual(panel,"upcomingTitle-0"),*rowArtist=visual(panel,"upcomingArtist-0");QVERIFY(rowTitle);QVERIFY(rowArtist);
            QVERIFY(rect(rowTitle).right()<rect(rowArtist).left());QVERIFY(rect(rowArtist).right()<=rect(panel).right());QCOMPARE(panel->property("rowHeight").toInt(),46);QCOMPARE(panel->property("coverSize").toInt(),36);
            shot(w,QString("right-queue-%1").arg(width));QVERIFY(click(w,visual(panel,"upcomingToggle")));QTest::qWait(350);
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void searchExpandsWithoutReservingTitleSpaceAndKeepsItsQuery() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        w->resize(414,790);QTest::qWait(420);auto *control=visual(root,"librarySearchControl");QVERIFY(control);
        auto *button=visual(root,"librarySearchButton");QVERIFY(button);QCOMPARE(qRound(control->width()),36);QVERIFY(visual(root,"importMusicButton")->isVisible());shot(w,"search-collapsed-414");
        QVERIFY(click(w,button));QTest::qWait(300);
        auto *popup=w->findChild<QObject *>("librarySearchPopup");QVERIFY(popup);QVERIFY(popup->property("opened").toBool());
        auto *input=visual(root,"librarySearch");QVERIFY(input);QVERIFY(input->hasActiveFocus());QVERIFY(input->width()-input->property("leftPadding").toReal()-input->property("rightPadding").toReal()>=75);
        for(const char letter:QByteArray("Second"))QTest::keyClick(w,letter);QTRY_COMPARE(f.shell->libraryModel()->count(),1);QCOMPARE(f.shell->libraryModel()->filterText(),QString("Second"));shot(w,"search-glass-414");
        QVERIFY(visual(root,"clearLibrarySearch")->isVisible());
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(300,400));QTest::qWait(150);QVERIFY(!popup->property("visible").toBool());QCOMPARE(f.shell->libraryModel()->filterText(),QString("Second"));
        QVERIFY(click(w,button));QTest::qWait(300);input=visual(root,"librarySearch");QVERIFY(input);QCOMPARE(input->property("text").toString(),QString("Second"));
        QVERIFY(click(w,visual(root,"clearLibrarySearch")));QTRY_COMPARE(f.shell->libraryModel()->count(),2);QVERIFY(f.shell->libraryModel()->filterText().isEmpty());
        for(const char letter:QByteArray("Second"))QTest::keyClick(w,letter);QTest::keyClick(w,Qt::Key_Return);QTest::qWait(150);QVERIFY(!popup->property("visible").toBool());QCOMPARE(f.shell->libraryModel()->filterText(),QString("Second"));
        QVERIFY(f.shell->libraryBridge()->openPlaylist("-1"));QVERIFY(!visual(root,"importMusicButton")->isVisible());
        const auto key=f.shell->libraryBridge()->createPlaylist("搜索入口检查");QVERIFY(f.shell->libraryBridge()->openPlaylist(key));QVERIFY(!visual(root,"importMusicButton")->isVisible());
        QVERIFY(f.shell->libraryBridge()->openPlaylist("0"));QVERIFY(visual(root,"importMusicButton")->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void sidebarWidthIsManualAndSurvivesNarrowMode() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *side=visual(root,"sidebar");QVERIFY(side);
        for(bool condensed:{true,false}) {
            w->resize(900,790);QTest::qWait(420);QVERIFY(!visual(root,"sidebarWidthToggle"));QVERIFY(click(w,visual(root,"sidebarLogo")));QTest::qWait(340);
            QCOMPARE(w->property("sidebarCondensed").toBool(),condensed);QCOMPARE(qRound(side->width()),condensed?84:205);shot(w,condensed?"sidebar-condensed":"sidebar-standard");
            w->resize(600,790);QTest::qWait(420);QCOMPARE(qRound(side->width()),condensed?84:205);
            w->resize(414,790);QTest::qWait(420);QCOMPARE(qRound(side->width()),0);QCOMPARE(w->property("sidebarCondensed").toBool(),condensed);
            w->resize(900,790);QTest::qWait(420);QCOMPARE(qRound(side->width()),condensed?84:205);
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void settingsPlaylistsAndModeSelectionAreUsable() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        auto *mode=visual(root,"listPlayMode");QVERIFY(mode);QCOMPARE(mode->property("selectedMode").toInt(),1);
        QVERIFY(click(w,visual(mode,"listPlayModeMenuButton")));QTest::qWait(300);
        auto *menu=w->findChild<QObject *>("listPlayModes");QVERIFY(menu);QCOMPARE(menu->property("count").toInt(),4);
        auto *selected=visual(root,"listModeOption-1");QVERIFY(selected);
        auto *mark=qvariant_cast<QQuickItem *>(selected->property("indicator")),*caption=qvariant_cast<QQuickItem *>(selected->property("contentItem"));QVERIFY(mark);QVERIFY(caption);
        QVERIFY(rect(mark).right()<rect(caption).left());shot(w,"play-mode-menu");
        QVERIFY(click(w,visual(root,"listModeOption-0")));QTest::qWait(220);QCOMPARE(mode->property("selectedMode").toInt(),0);
        mode->setProperty("selectedMode",3);QVERIFY(click(w,visual(mode,"listPlayModeStart")));QTRY_COMPARE(int(f.shell->facade()->mode()),3);
        mode->setProperty("selectedMode",1);QVERIFY(click(w,visual(mode,"listPlayModeStart")));QTRY_COMPARE(int(f.shell->facade()->mode()),1);
        const auto key=f.shell->libraryBridge()->createPlaylist("晚间音乐");QVERIFY(key!="0");
        const auto second=f.shell->libraryBridge()->createPlaylist("晨间音乐"),third=f.shell->libraryBridge()->createPlaylist("旅途音乐");
        w->resize(460,790);QTest::qWait(420);QVERIFY(f.shell->appBridge()->navigate(1));QTest::qWait(160);
        auto *cards=visual(root,"playlistCards");QVERIFY(cards);QCOMPARE(cards->property("cellWidth").toReal()*2,cards->width());
        auto *firstCover=visual(root,"playlistCardCover-"+key),*secondCover=visual(root,"playlistCardCover-"+second),*thirdCover=visual(root,"playlistCardCover-"+third);
        QVERIFY(firstCover);QVERIFY(secondCover);QVERIFY(thirdCover);
        QCOMPARE(rect(firstCover).top(),rect(secondCover).top());QVERIFY(rect(secondCover).left()>rect(firstCover).right());QVERIFY(rect(thirdCover).top()>rect(firstCover).bottom());
        for(int width:{900,460,414,460}) {
            w->resize(width,790);
            for(int frame=0;frame<25;++frame) {
                QTest::qWait(16);w->grabWindow();
                auto *art=visual(root,"playlistCardCover-"+key),*title=visual(root,"playlistCardTitle-"+key);QVERIFY(art);QVERIFY(title);
                QVERIFY2(art->width()+14<=cards->property("cellWidth").toReal()+.5,"Playlist artwork crosses into the adjacent column");
                QVERIFY2(!rect(art).intersects(rect(title)),"Playlist cover overlaps title during adaptive animation");
                QVERIFY(rect(title).left()>=0);QVERIFY(rect(title).right()<=w->width());
            }
        }
        shot(w,"playlists-460");
        QVERIFY(f.shell->libraryBridge()->openPlaylist(key));QVERIFY(f.shell->appBridge()->navigate(0));QTest::qWait(250);
        auto *playlistMode=visual(root,"playlistMode");QVERIFY(playlistMode);QVERIFY(rect(playlistMode).right()<=w->width());
        QVERIFY(!visual(root,"playlistUp")->isVisible());QVERIFY(!visual(root,"playlistDown")->isVisible());QVERIFY(!visual(root,"playlistDescriptionRow")->isVisible());
        auto *options=visual(root,"playlistOptions"),*statistics=visual(root,"playlistStatisticsRow");QVERIFY(options);QVERIFY(statistics);
        QVERIFY(qAbs(rect(options).center().y()-rect(statistics).center().y())<1);QVERIFY(rect(statistics).left()>rect(options).right());
        QVERIFY(qAbs(rect(visual(root,"playlistStatisticsContent")).right()-rect(visual(root,"playlistHeader")).right())<1);
        shot(w,"playlist-detail-460");
        w->resize(414,790);QTest::qWait(420);QVERIFY(rect(playlistMode).right()<=w->width()+1);shot(w,"playlist-detail-414");w->resize(460,790);QTest::qWait(420);
        QVERIFY(click(w,options));QTest::qWait(300);auto *playlistMenu=w->findChild<QObject *>("playlistOptionsMenu");QVERIFY(playlistMenu);QVERIFY(playlistMenu->property("opened").toBool());
        QCOMPARE(playlistMenu->property("popupType").toInt(),0);QVERIFY(visual(root,"playlistMoveUp")->isVisible());QVERIFY(visual(root,"playlistMoveDown")->isVisible());shot(w,"playlist-options-460");
        QVERIFY(QMetaObject::invokeMethod(playlistMenu,"close"));QTest::qWait(220);
        QVERIFY(click(w,visual(root,"compactNavigation")));QTest::qWait(300);
        QVERIFY(click(w,visual(root,"compactNavigate-3")));QTest::qWait(550);QCOMPARE(f.shell->appBridge()->page(),3);
        QVERIFY(f.shell->appBridge()->navigate(3));QTest::qWait(250);
        QVERIFY(visual(root,"lyricColorSetting")->isVisible());QVERIFY(!visual(root,"lyricColorReset"));
        const auto theme=rect(visual(root,"qmlThemeSetting")),threshold=rect(visual(root,"qmlThresholdSetting")),close=rect(visual(root,"closeSetting"));
        QVERIFY(threshold.top()>theme.bottom());QVERIFY(close.top()>threshold.bottom());shot(w,"settings-460");
        auto *scroll=visual(root,"settingsScroll");QVERIFY(scroll);auto *flick=qvariant_cast<QQuickItem *>(scroll->property("contentItem"));QVERIFY(flick);
        auto *shortcuts=visual(root,"shortcutsPage");QVERIFY(shortcuts);flick->setProperty("contentY",shortcuts->y());QTest::qWait(100);shot(w,"shortcuts-460");
        auto *app=visual(root,"shortcut-app-toggle"),*global=visual(root,"shortcut-global-toggle");
        QVERIFY(app);QVERIFY(global);QVERIFY(rect(global).top()>rect(app).bottom());QVERIFY(rect(global).right()<=460);
        w->resize(1240,790);QTest::qWait(420);flick->setProperty("contentY",0);shot(w,"settings-wide-restored");
        f.shell->settingsBridge()->beginAppearance();w->setProperty("settingsSection","appearance");QTest::qWait(100);
        QVERIFY(f.shell->settingsBridge()->setAppearanceMode(3));QVERIFY(f.shell->settingsBridge()->editAppearance("right","#335577"));
        const auto draft=f.shell->settingsBridge()->appearanceDraft();
        w->resize(460,790);QTest::qWait(420);w->resize(1240,790);QTest::qWait(420);
        QVERIFY(f.shell->settingsBridge()->appearanceDirty());QCOMPARE(f.shell->settingsBridge()->appearanceDraft(),draft);
        f.shell->settingsBridge()->discardAppearance();
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void playlistFiltersSlideInAndResetWhenLeaving() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        const auto key=f.shell->libraryBridge()->createPlaylist("查询歌单"),other=f.shell->libraryBridge()->createPlaylist("其他歌单");
        QVERIFY(f.shell->libraryBridge()->addTracksToPlaylist(key,{QString::number(f.a.id),QString::number(f.b.id)}));
        QVERIFY(f.shell->libraryBridge()->openPlaylist(key));w->grabWindow();
        QVERIFY(!visual(root,"libraryFilterRow")->isVisible());
        w->resize(414,790);QTest::qWait(420);
        auto *page=visual(root,"localPage"),*table=visual(root,"libraryListContainer");QVERIFY(page);QVERIFY(table);
        auto *heading=visual(root,"libraryHeading"),*cover=visual(root,"playlistCover");QVERIFY(heading);QVERIFY(cover);
        QVERIFY(rect(cover).top()>=rect(heading).bottom());QVERIFY(rect(cover).top()-rect(heading).bottom()<=10);
        const auto closedTop=rect(table).top();
        auto openFilters=[&]{
            if(!click(w,visual(root,"playlistOptions")))return false;
            QTest::qWait(260);
            auto *menu=w->findChild<QObject *>("playlistOptionsMenu");if(!menu)return false;
            for(auto *item:menu->findChildren<QObject *>())if(item->property("text").toString()=="查找缺失歌词")return false;
            return click(w,visual(root,"playlistFilterEntry"));
        };
        QVERIFY(openFilters());QTest::qWait(70);QVERIFY(rect(table).top()>closedTop);QVERIFY(rect(table).top()<closedTop+44);QTest::qWait(300);
        QVERIFY(page->property("optionsExpanded").toBool());
        auto *artist=visual(root,"artistFilter"),*album=visual(root,"albumFilter"),*collapse=visual(root,"collapsePlaylistFilters");QVERIFY(artist);QVERIFY(album);QVERIFY(collapse);
        QVERIFY(artist->isVisible());QVERIFY(album->isVisible());QVERIFY(collapse->isVisible());QVERIFY(!visual(root,"findMissingLyrics")->isVisible());
        QVERIFY(rect(artist).right()<rect(album).left());QVERIFY(rect(album).right()<rect(collapse).left());QVERIFY(rect(collapse).right()<=w->width()-18);
        shot(w,"playlist-filters-414");
        QVERIFY(click(w,artist));QTest::qWait(260);QTest::keyClick(w,Qt::Key_End);QTest::keyClick(w,Qt::Key_Return);
        QTRY_COMPARE(f.shell->libraryModel()->artistFilter(),QString("Test Artist"));QCOMPARE(f.shell->libraryModel()->count(),2);
        QVERIFY(click(w,collapse));QTest::qWait(350);QVERIFY(!page->property("optionsExpanded").toBool());QVERIFY(qAbs(rect(table).top()-closedTop)<1);
        QVERIFY(openFilters());QTest::qWait(350);
        QVERIFY(f.shell->appBridge()->navigate(1));QTest::qWait(100);QVERIFY(!page->property("optionsExpanded").toBool());
        QVERIFY(f.shell->appBridge()->navigate(0));QTest::qWait(350);QVERIFY(!page->property("optionsExpanded").toBool());
        QVERIFY(openFilters());QTest::qWait(350);QVERIFY(f.shell->libraryBridge()->openPlaylist(other));QTest::qWait(350);QVERIFY(!page->property("optionsExpanded").toBool());
        w->resize(1240,790);QTest::qWait(420);QVERIFY(openFilters());QTest::qWait(350);
        QVERIFY(page->property("optionsExpanded").toBool());QVERIFY(rect(table).top()>rect(artist).bottom());shot(w,"playlist-filters-wide");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
};
int main(int argc,char **argv) { QApplication app(argc,argv);AdaptiveLayoutTests tests;return QTest::qExec(&tests,argc,argv); }
#include "qml-adaptive-layout-tests.moc"
