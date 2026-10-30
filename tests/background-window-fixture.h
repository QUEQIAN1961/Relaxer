#pragma once
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// A thread-local test fixture, not a desktop/global hook. Block only this
// test thread's programmatic activation; never activate or alter another app.
// Explicit user mouse activation is allowed and is observed normally.
class BackgroundWindowFixture {
#ifdef Q_OS_WIN
    HHOOK hook=nullptr;
    static LRESULT CALLBACK activation(int code,WPARAM window,LPARAM value){
        if(code==HCBT_ACTIVATE){
            const auto *request=reinterpret_cast<const CBTACTIVATESTRUCT *>(value);
            if(!request||!request->fMouse)return 1;
        }
        return CallNextHookEx(nullptr,code,window,value);
    }
#endif
public:
    BackgroundWindowFixture(){
#ifdef Q_OS_WIN
        hook=SetWindowsHookExW(WH_CBT,activation,nullptr,GetCurrentThreadId());
#endif
    }
    bool installed() const {
#ifdef Q_OS_WIN
        return hook!=nullptr;
#else
        return true;
#endif
    }
    ~BackgroundWindowFixture(){
#ifdef Q_OS_WIN
        if(hook)UnhookWindowsHookEx(hook);
#endif
    }
};
