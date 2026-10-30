#pragma once
#include <QCoreApplication>
#include <QPointer>
#include <QPlatformSurfaceEvent>
#include <QWindow>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// Test-thread policies only; never activate or alter another application.
// The foreground-observation case uses Qt's initial non-activating show policy
// and removes it before input. Legacy visual/tray cases retain their CBT guard.
class BackgroundWindowFixture : public QObject {
public:
    enum class Mode { BlockProgrammaticActivation, MakeNewWindowsNonActivating };
private:
    Mode mode;
    bool filterInstalled=false;
    QList<QPair<QPointer<QWindow>,Qt::WindowFlags>> windows;
    mutable int marked=0,denied=0,styleFailures=0;
    bool eventFilter(QObject *object,QEvent *event) override {
        if(mode==Mode::MakeNewWindowsNonActivating&&event->type()==QEvent::PlatformSurface&&
           static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()==QPlatformSurfaceEvent::SurfaceCreated){
            if(auto *window=qobject_cast<QWindow *>(object)){
                if(!window->flags().testFlag(Qt::WindowDoesNotAcceptFocus)){
                    const auto original=window->flags();windows.append({window,original});
                    // Qt chooses a non-activating initial show policy. Unlike
                    // an HCBT_ACTIVATE veto this does not cancel an in-flight
                    // Windows foreground transition.
                    window->setFlag(Qt::WindowDoesNotAcceptFocus,true);
                    if(window->flags().testFlag(Qt::WindowDoesNotAcceptFocus))++marked;else ++styleFailures;
                }
            }
        }
        return false;
    }
#ifdef Q_OS_WIN
    HHOOK hook=nullptr;
    BackgroundWindowFixture *previous=nullptr;
    inline static thread_local BackgroundWindowFixture *active=nullptr;
    static LRESULT CALLBACK activation(int code,WPARAM window,LPARAM value){
        if(active&&active->mode==Mode::BlockProgrammaticActivation&&code==HCBT_ACTIVATE){
            const auto *request=reinterpret_cast<const CBTACTIVATESTRUCT *>(value);
            if(!request||!request->fMouse){++active->denied;return 1;}
        }
        return CallNextHookEx(nullptr,code,window,value);
    }
#endif
public:
    explicit BackgroundWindowFixture(Mode selected=Mode::BlockProgrammaticActivation)
        :mode(selected)
    {
        if(mode==Mode::MakeNewWindowsNonActivating){
            if(QCoreApplication::instance()){QCoreApplication::instance()->installEventFilter(this);filterInstalled=true;}
            return;
        }
#ifdef Q_OS_WIN
        previous=active;active=this;
        hook=SetWindowsHookExW(WH_CBT,activation,nullptr,GetCurrentThreadId());
#endif
    }
    bool installed() const {
        if(mode==Mode::MakeNewWindowsNonActivating)return filterInstalled;
#ifdef Q_OS_WIN
        return hook!=nullptr;
#else
        return true;
#endif
    }
    int markedWindows() const {
        return marked;
    }
    int activationVetoes() const {
        return denied;
    }
    int markingFailures() const {
        return styleFailures;
    }
    void release(){
        if(filterInstalled){
            QCoreApplication::instance()->removeEventFilter(this);filterInstalled=false;
            for(const auto &entry:windows)if(entry.first)entry.first->setFlags(entry.second);
            windows.clear();
        }
#ifdef Q_OS_WIN
        if(hook){UnhookWindowsHookEx(hook);hook=nullptr;}
        if(active==this)active=previous;
#endif
    }
    ~BackgroundWindowFixture(){release();}
};
