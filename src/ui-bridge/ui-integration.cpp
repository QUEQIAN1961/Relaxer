#include "ui-integration.h"
#include "shortcut-rules.h"
#include <QApplication>
#include <QQuickItem>
#include <QAction>
#include <QIcon>
#include <QKeyEvent>
#include <QPlatformSurfaceEvent>
#include <QScreen>
#include <QMouseEvent>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif
namespace relaxer {
UiIntegration::UiIntegration(Library *library,PlaybackBridge *playback,QObject *parent):QObject(parent),m_library(library),m_playback(playback){static int base=0x5300;m_base=base;base+=8;QCoreApplication::instance()->installNativeEventFilter(this);QCoreApplication::instance()->installEventFilter(this);}
UiIntegration::~UiIntegration(){releaseGlobals();QCoreApplication::instance()->removeEventFilter(this);QCoreApplication::instance()->removeNativeEventFilter(this);}
bool UiIntegration::trayAvailable()const{return QSystemTrayIcon::isSystemTrayAvailable();}
void UiIntegration::attach(QQuickWindow *window){
    if(m_window==window)return;m_window=window;
    connect(window,&QWindow::activeChanged,this,&UiIntegration::updateWindowShadow);
    connect(window,&QWindow::visibilityChanged,this,&UiIntegration::updateWindowShadow);
#if QT_VERSION >= QT_VERSION_CHECK(6,10,0)
    connect(window,&QWindow::flagsChanged,this,&UiIntegration::updateWindowShadow);
#endif
    updateWindowShadow();
    if(qobject_cast<QApplication *>(QCoreApplication::instance())){
        m_menu=std::make_unique<QMenu>();
        connect(m_menu->addAction("打开 Relaxer"),&QAction::triggered,this,[this]{if(m_window){m_window->show();m_window->raise();m_window->requestActivate();}});
        connect(m_menu->addAction("播放 / 暂停"),&QAction::triggered,this,[this]{dispatch(0);});
        connect(m_menu->addAction("上一首"),&QAction::triggered,this,[this]{dispatch(1);});
        connect(m_menu->addAction("下一首"),&QAction::triggered,this,[this]{dispatch(2);});
        m_menu->addSeparator();connect(m_menu->addAction("退出播放器"),&QAction::triggered,this,[this]{if(m_window)QMetaObject::invokeMethod(m_window,"requestQuit");});
        m_tray=std::make_unique<QSystemTrayIcon>(QIcon(":/icon.svg"));m_tray->setToolTip("Relaxer");m_tray->setContextMenu(m_menu.get());
        connect(m_tray.get(),&QSystemTrayIcon::activated,this,[this](auto reason){if(reason==QSystemTrayIcon::Trigger||reason==QSystemTrayIcon::DoubleClick){m_window->show();m_window->raise();m_window->requestActivate();}});
    }
    QString error;apply(m_library->setting("shortcuts",ShortcutRules::defaults()).toObject(),&error);if(!error.isEmpty()){m_status=error;emit changed();}
}
bool UiIntegration::eventFilter(QObject *object,QEvent *event){
    if(object==m_window&&(event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonRelease))
        m_pointerPosition=static_cast<QMouseEvent *>(event)->globalPosition();
    if(object==m_window&&event->type()==QEvent::PlatformSurface&&
       static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()==QPlatformSurfaceEvent::SurfaceCreated)
        updateWindowShadow();
    const bool keyEvent=event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress;
    if(keyEvent&&!m_recording){
        auto *owner=qobject_cast<QWindow *>(object);
        while(owner&&owner!=m_window)owner=owner->transientParent();
        auto *key=static_cast<QKeyEvent *>(event);
        if(owner&&!m_miniSequence.isEmpty()&&QKeySequence(key->keyCombination())==m_miniSequence&&
           (!editableFocus()||(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier)))){
            if(event->type()==QEvent::KeyPress&&!key->isAutoRepeat())emit miniPlayerToggleRequested();
            key->accept();return true;
        }
    }
    if(object!=m_window || m_recording || !keyEvent)
        return QObject::eventFilter(object,event);
    auto *key=static_cast<QKeyEvent *>(event);
    const auto sequence=QKeySequence(key->keyCombination());
    for(const auto &binding:m_bindings){
        if(binding.sequence!=sequence || (!binding.media&&editableFocus()))continue;
        if(event->type()==QEvent::KeyPress && (!key->isAutoRepeat()||binding.action>=3))dispatch(binding.action);
        key->accept();return true;
    }
    return QObject::eventFilter(object,event);
}
void UiIntegration::updateWindowShadow(){
#ifdef Q_OS_WIN
    if(!m_window||!m_window->handle()||QGuiApplication::platformName()!=QStringLiteral("windows"))return;
    const auto window=reinterpret_cast<HWND>(m_window->winId());
    const bool active=m_window->isActive()&&m_window->isVisible()&&m_window->visibility()==QWindow::Windowed;
    const DWMNCRENDERINGPOLICY policy=active?DWMNCRP_ENABLED:DWMNCRP_DISABLED;
    DwmSetWindowAttribute(window,DWMWA_NCRENDERING_POLICY,&policy,sizeof(policy));
    // Extend only the compositor frame; QML keeps its full client area.
    const int edge=active?1:0;const MARGINS margins{edge,edge,edge,edge};
    DwmExtendFrameIntoClientArea(window,&margins);
#endif
}
void UiIntegration::showTray(){if(m_tray&&trayAvailable())m_tray->show();}
void UiIntegration::setWindowGeometry(qreal x,qreal y,qreal width,qreal height){
#ifdef Q_OS_WIN
    if(m_window&&m_window->handle()&&QGuiApplication::platformName()==QStringLiteral("windows")){
        const auto handle=reinterpret_cast<HWND>(m_window->winId());
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        RECT current{};const auto *screen=m_window->screen();
        if(screen&&GetWindowRect(handle,&current)&&GetMonitorInfo(MonitorFromWindow(handle,MONITOR_DEFAULTTONEAREST),&monitor)){
            // Keep fractional animation coordinates until the final native-pixel
            // conversion. Rounding first in QML, then again for DPI made even a
            // keyboard transition's horizontal center oscillate by 1-1.5 pixels.
            const auto ratio=m_window->devicePixelRatio();
            const int w=qRound(qBound(qreal(m_window->minimumWidth()),width,qreal(m_window->maximumWidth()))*ratio);
            const int h=qRound(qBound(qreal(m_window->minimumHeight()),height,qreal(m_window->maximumHeight()))*ratio);
            const auto origin=screen->geometry().topLeft();
            int left=monitor.rcMonitor.left+qRound((x-origin.x())*ratio);
            int top=monitor.rcMonitor.top+qRound((y-origin.y())*ratio);
            // A fixed physical edge can lie between Qt's integer logical pixels.
            // Derive its moving origin from size in this same native transaction.
            if(m_resizeWindow==WId(handle)){
                left=m_resizeEdges&Qt::LeftEdge?m_nativeResizeAnchor.x()-w:m_resizeEdges&Qt::RightEdge?m_nativeResizeAnchor.x():m_nativeResizeAnchor.x()-w/2;
                top=m_resizeEdges&Qt::TopEdge?m_nativeResizeAnchor.y()-h:m_nativeResizeAnchor.y();
            }
            if((current.left==left&&current.top==top&&current.right-current.left==w&&current.bottom-current.top==h)||
               SetWindowPos(handle,nullptr,left,top,w,h,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER))return;
        }
        endResizeAnchor();
    }
#endif
    const QRect bounds(qRound(x),qRound(y),qRound(width),qRound(height));
    if(m_window&&m_window->geometry()!=bounds)m_window->setGeometry(bounds);
}
void UiIntegration::beginResizeAnchor(int edges){
    m_resizeEdges=edges;
#ifdef Q_OS_WIN
    RECT r{};
    m_resizeWindow=m_window&&QGuiApplication::platformName()==QStringLiteral("windows")?m_window->winId():0;
    if(m_resizeWindow&&GetWindowRect(reinterpret_cast<HWND>(m_resizeWindow),&r))
        m_nativeResizeAnchor={int(edges&Qt::LeftEdge?r.right:edges&Qt::RightEdge?r.left:(r.left+r.right)/2),int(edges&Qt::TopEdge?r.bottom:r.top)};
    else m_resizeWindow=0;
#endif
}
void UiIntegration::endResizeAnchor(){m_resizeEdges=0;m_resizeWindow=0;}
QPoint UiIntegration::clampWindowPosition(int x,int y,int width,int height)const{
    auto *screen=QGuiApplication::screenAt(QPoint(x+width/2,y+height/2));
    if(!screen)screen=m_window?m_window->screen():QGuiApplication::primaryScreen();
    if(!screen)return {x,y};
    const auto bounds=screen->availableGeometry();
    return {qBound(bounds.left(),x,qMax(bounds.left(),bounds.right()+1-width)),qBound(bounds.top(),y,qMax(bounds.top(),bounds.bottom()+1-height))};
}
void UiIntegration::saveMiniWindowPlacement(){
    m_miniPlacement.clear();
#ifdef Q_OS_WIN
    if(!m_window||m_window->visibility()==QWindow::FullScreen)return;
    WINDOWPLACEMENT p{};p.length=sizeof(p);
    if(GetWindowPlacement(reinterpret_cast<HWND>(m_window->winId()),&p))
        m_miniPlacement={int(p.flags),int(p.showCmd),int(p.ptMinPosition.x),int(p.ptMinPosition.y),int(p.ptMaxPosition.x),int(p.ptMaxPosition.y),int(p.rcNormalPosition.left),int(p.rcNormalPosition.top),int(p.rcNormalPosition.right),int(p.rcNormalPosition.bottom)};
#endif
}
void UiIntegration::restoreMiniWindowPlacement(){
#ifdef Q_OS_WIN
    if(m_window&&m_miniPlacement.size()==10){
        WINDOWPLACEMENT p{};p.length=sizeof(p);p.flags=m_miniPlacement[0].toUInt();p.showCmd=m_miniPlacement[1].toUInt();
        p.ptMinPosition={m_miniPlacement[2].toInt(),m_miniPlacement[3].toInt()};p.ptMaxPosition={m_miniPlacement[4].toInt(),m_miniPlacement[5].toInt()};
        p.rcNormalPosition={m_miniPlacement[6].toInt(),m_miniPlacement[7].toInt(),m_miniPlacement[8].toInt(),m_miniPlacement[9].toInt()};
        SetWindowPlacement(reinterpret_cast<HWND>(m_window->winId()),&p);
    }
#endif
    m_miniPlacement.clear();
}
bool UiIntegration::editableFocus()const{
    for(auto *item=m_window?m_window->activeFocusItem():nullptr;item;item=item->parentItem()){
        for(auto *meta=item->metaObject();meta;meta=meta->superClass()){const QByteArray type=meta->className();if(type.contains("TextInput")||type.contains("TextEdit")||type.contains("TextField")||type.contains("SpinBox")||type.contains("ComboBox"))return true;}
    }return false;
}
void UiIntegration::dispatch(int action){if(!m_playback||m_recording)return;switch(action){case 0:m_playback->toggle();break;case 1:m_playback->previous();break;case 2:m_playback->next();break;case 3:m_playback->setVolume(qMin(100,m_playback->volume()+5));break;case 4:m_playback->setVolume(qMax(0,m_playback->volume()-5));break;case 5:emit miniPlayerToggleRequested();break;}}
void UiIntegration::releaseGlobals(){
#ifdef Q_OS_WIN
    for(auto id:m_registered)UnregisterHotKey(nullptr,id);
#endif
    m_registered.clear();
}
bool UiIntegration::registerGlobals(const QJsonObject &config,QString *error){
    if(!config["globalEnabled"].toBool()||m_recording)return true;
#ifdef Q_OS_WIN
    const auto keys=ShortcutRules::actionKeys()+QStringList{"miniPlayer"};for(int i=0;i<keys.size();++i){const auto sequence=QKeySequence::fromString(ShortcutRules::withMiniPlayerBinding(config)["global"].toObject()[keys[i]].toString(),QKeySequence::PortableText);if(sequence.isEmpty())continue;const auto key=ShortcutRules::nativeKey(sequence);
        if(!RegisterHotKey(nullptr,m_base+i,key.modifiers|((i==3||i==4)?0:MOD_NOREPEAT),key.key)){releaseGlobals();if(error)*error="全局快捷键被其他应用占用："+sequence.toString(QKeySequence::NativeText);return false;}m_registered.append(m_base+i);}
    return true;
#else
    if(error)*error="当前系统不支持 Windows 全局快捷键";return false;
#endif
}
bool UiIntegration::apply(const QJsonObject &config,QString *error){
    if(!m_window||!ShortcutRules::validate(config,error))return false;
    releaseGlobals();if(!registerGlobals(config,error)){QString ignored;registerGlobals(m_config,&ignored);return false;}
    m_bindings.clear();const auto keys=ShortcutRules::actionKeys();
    for(int i=0;i<keys.size();++i){
        const auto sequence=QKeySequence::fromString(config["app"].toObject()[keys[i]].toString(),QKeySequence::PortableText);
        if(!sequence.isEmpty())m_bindings.append({sequence,i,false});
    }
    const QList<Qt::Key> media{Qt::Key_MediaTogglePlayPause,Qt::Key_MediaPrevious,Qt::Key_MediaNext};
    for(int i=0;i<media.size();++i){
        const QKeySequence sequence(media[i]);bool assigned=false;
        for(const auto &binding:m_bindings)assigned|=binding.sequence==sequence;
        for(const auto &name:keys)assigned|=config["globalEnabled"].toBool()&&QKeySequence::fromString(config["global"].toObject()[name].toString(),QKeySequence::PortableText)==sequence;
        if(!assigned)m_bindings.append({sequence,i,true});
    }
    m_miniSequence=QKeySequence::fromString(ShortcutRules::withMiniPlayerBinding(config)["app"].toObject()["miniPlayer"].toString(),QKeySequence::PortableText);
    m_config=config;m_status=config["globalEnabled"].toBool()?"应用内与全局快捷键已生效":"应用内快捷键已生效；全局快捷键未启用";emit changed();return true;
}
bool UiIntegration::saveShortcuts(const QVariantMap &configuration){const auto next=QJsonObject::fromVariantMap(configuration);const auto old=m_config;QString error;if(!apply(next,&error)){m_status=error;emit changed();return false;}if(!m_library->setSetting("shortcuts",next)){QString ignored;apply(old,&ignored);m_status=m_library->lastError();emit changed();return false;}return true;}
void UiIntegration::setShortcutRecording(bool recording){if(m_recording==recording)return;m_recording=recording;releaseGlobals();if(!recording){QString error;if(!registerGlobals(m_config,&error)){m_status=error;emit changed();}}}
bool UiIntegration::nativeEventFilter(const QByteArray &,void *message,qintptr *result){
#ifdef Q_OS_WIN
    const auto *event=static_cast<MSG *>(message);
    if(event->message==WM_ERASEBKGND&&m_window&&m_window->handle()&&event->hwnd==reinterpret_cast<HWND>(m_window->winId())&&m_window->property("miniSwitching").toBool()){
        const auto color=m_window->color();const auto brush=CreateSolidBrush(RGB(color.red(),color.green(),color.blue()));
        RECT client{};GetClientRect(event->hwnd,&client);FillRect(reinterpret_cast<HDC>(event->wParam),&client,brush);DeleteObject(brush);
        if(result)*result=1;return true;
    }
    if(event->message==WM_HOTKEY&&m_registered.contains(int(event->wParam))&&!m_recording){dispatch(int(event->wParam)-m_base);if(result)*result=0;return true;}
#else
    Q_UNUSED(message);Q_UNUSED(result);
#endif
    return false;
}
}
