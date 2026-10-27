#include "ui-integration.h"
#include "shortcut-rules.h"
#include <QApplication>
#include <QQuickItem>
#include <QAction>
#include <QIcon>
#include <QKeyEvent>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace relaxer {
UiIntegration::UiIntegration(Library *library,PlaybackBridge *playback,QObject *parent):QObject(parent),m_library(library),m_playback(playback){static int base=0x5300;m_base=base;base+=8;QCoreApplication::instance()->installNativeEventFilter(this);}
UiIntegration::~UiIntegration(){releaseGlobals();QCoreApplication::instance()->removeNativeEventFilter(this);}
bool UiIntegration::trayAvailable()const{return QSystemTrayIcon::isSystemTrayAvailable();}
void UiIntegration::attach(QQuickWindow *window){
    if(m_window==window)return;m_window=window;
    window->installEventFilter(this);
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
    if(object!=m_window || m_recording || (event->type()!=QEvent::ShortcutOverride && event->type()!=QEvent::KeyPress))
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
void UiIntegration::showTray(){if(m_tray&&trayAvailable())m_tray->show();}
bool UiIntegration::editableFocus()const{
    for(auto *item=m_window?m_window->activeFocusItem():nullptr;item;item=item->parentItem()){
        for(auto *meta=item->metaObject();meta;meta=meta->superClass()){const QByteArray type=meta->className();if(type.contains("TextInput")||type.contains("TextEdit")||type.contains("TextField")||type.contains("SpinBox")||type.contains("ComboBox"))return true;}
    }return false;
}
void UiIntegration::dispatch(int action){if(!m_playback||m_recording)return;switch(action){case 0:m_playback->toggle();break;case 1:m_playback->previous();break;case 2:m_playback->next();break;case 3:m_playback->setVolume(qMin(100,m_playback->volume()+5));break;case 4:m_playback->setVolume(qMax(0,m_playback->volume()-5));break;}}
void UiIntegration::releaseGlobals(){
#ifdef Q_OS_WIN
    for(auto id:m_registered)UnregisterHotKey(nullptr,id);
#endif
    m_registered.clear();
}
bool UiIntegration::registerGlobals(const QJsonObject &config,QString *error){
    if(!config["globalEnabled"].toBool()||m_recording)return true;
#ifdef Q_OS_WIN
    const auto keys=ShortcutRules::actionKeys();for(int i=0;i<keys.size();++i){const auto sequence=QKeySequence::fromString(config["global"].toObject()[keys[i]].toString(),QKeySequence::PortableText);if(sequence.isEmpty())continue;const auto key=ShortcutRules::nativeKey(sequence);
        if(!RegisterHotKey(nullptr,m_base+i,key.modifiers|(i>=3?0:MOD_NOREPEAT),key.key)){releaseGlobals();if(error)*error="全局快捷键被其他应用占用："+sequence.toString(QKeySequence::NativeText);return false;}m_registered.append(m_base+i);}
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
    m_config=config;m_status=config["globalEnabled"].toBool()?"应用内与全局快捷键已生效":"应用内快捷键已生效；全局快捷键未启用";emit changed();return true;
}
bool UiIntegration::saveShortcuts(const QVariantMap &configuration){const auto next=QJsonObject::fromVariantMap(configuration);const auto old=m_config;QString error;if(!apply(next,&error)){m_status=error;emit changed();return false;}if(!m_library->setSetting("shortcuts",next)){QString ignored;apply(old,&ignored);m_status=m_library->lastError();emit changed();return false;}return true;}
void UiIntegration::setShortcutRecording(bool recording){if(m_recording==recording)return;m_recording=recording;releaseGlobals();if(!recording){QString error;if(!registerGlobals(m_config,&error)){m_status=error;emit changed();}}}
bool UiIntegration::nativeEventFilter(const QByteArray &,void *message,qintptr *result){
#ifdef Q_OS_WIN
    const auto *event=static_cast<MSG *>(message);if(event->message==WM_HOTKEY&&m_registered.contains(int(event->wParam))&&!m_recording){dispatch(int(event->wParam)-m_base);if(result)*result=0;return true;}
#else
    Q_UNUSED(message);Q_UNUSED(result);
#endif
    return false;
}
}
