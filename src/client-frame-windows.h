#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
namespace relaxer::client_frame {
inline LONG_PTR windowStyle(LONG_PTR style) {
    return (style & ~WS_CAPTION) | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
}
inline void theme(HWND window,bool dark) {
    const DWMNCRENDERINGPOLICY policy=DWMNCRP_DISABLED;
    DwmSetWindowAttribute(window,DWMWA_NCRENDERING_POLICY,&policy,sizeof(policy));
    const BOOL enabled=dark?TRUE:FALSE;
    // Newer attributes are optional; Windows 10/11 differ in support.
    DwmSetWindowAttribute(window,20,&enabled,sizeof(enabled));
    constexpr COLORREF noBorder=0xfffffffe;
    DwmSetWindowAttribute(window,34,&noBorder,sizeof(noBorder));
}
inline void initialize(HWND window,bool dark) {
    SetWindowLongPtrW(window,GWL_STYLE,windowStyle(GetWindowLongPtrW(window,GWL_STYLE)));
    theme(window,dark);
    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
}
inline LRESULT resizeHit(const RECT &rect,POINT p,int margin,bool maximized) {
    if(maximized)return HTCLIENT;
    const bool left=p.x<rect.left+margin,right=p.x>=rect.right-margin;
    const bool top=p.y<rect.top+margin,bottom=p.y>=rect.bottom-margin;
    if(top&&left)return HTTOPLEFT;
    if(top&&right)return HTTOPRIGHT;
    if(bottom&&left)return HTBOTTOMLEFT;
    if(bottom&&right)return HTBOTTOMRIGHT;
    if(left)return HTLEFT;
    if(right)return HTRIGHT;
    if(top)return HTTOP;
    if(bottom)return HTBOTTOM;
    return HTCLIENT;
}
inline void constrainMaximized(HWND window,RECT &rect) {
    MONITORINFO info{};info.cbSize=sizeof(info);
    if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&info))rect=info.rcWork;
}
}
#endif
