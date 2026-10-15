#include "client-frame-windows.h"
#include <iostream>
using namespace relaxer;
static LRESULT CALLBACK frameProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
    if(message==WM_NCCALCSIZE){auto *rect=wParam?&reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam)->rgrc[0]:reinterpret_cast<RECT *>(lParam);if(IsZoomed(window))client_frame::constrainMaximized(window,*rect);return 0;}
    return DefWindowProcW(window,message,wParam,lParam);
}
int wmain(){
    int failures=0;const auto check=[&](bool value,const char *name){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';if(!value)++failures;};
    RECT rect{0,0,400,260};
    check(client_frame::resizeHit(rect,{1,1},6,false)==HTTOPLEFT,"top-left resize");
    check(client_frame::resizeHit(rect,{399,259},6,false)==HTBOTTOMRIGHT,"bottom-right resize");
    check(client_frame::resizeHit(rect,{200,1},6,false)==HTTOP,"top resize");
    check(client_frame::resizeHit(rect,{1,150},6,false)==HTLEFT,"left resize");
    check(client_frame::resizeHit(rect,{200,150},6,false)==HTCLIENT,"client hit");
    check(client_frame::resizeHit(rect,{1,1},6,true)==HTCLIENT,"maximized disables resize");
    WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=frameProcedure;cls.lpszClassName=L"RelaxerClientFrameTest";
    check(RegisterClassW(&cls)!=0,"register frame fixture");
    auto window=CreateWindowW(cls.lpszClassName,L"metadata only",WS_OVERLAPPEDWINDOW,40,40,400,260,nullptr,nullptr,cls.hInstance,nullptr);
    check(window!=nullptr,"create native window");if(!window)return 1;
    client_frame::initialize(window,false);const auto style=GetWindowLongPtrW(window,GWL_STYLE);
    check((style&WS_CAPTION)==0,"native caption removed");
    check((style&WS_THICKFRAME)!=0&&(style&WS_MAXIMIZEBOX)!=0&&(style&WS_MINIMIZEBOX)!=0,"system sizing and controls retained");
    RECT outer{},client{};GetWindowRect(window,&outer);GetClientRect(window,&client);
    check(client.right==outer.right-outer.left&&client.bottom==outer.bottom-outer.top,"client fills normal window");
    ShowWindow(window,SW_MAXIMIZE);UpdateWindow(window);
    RECT maximized{};GetClientRect(window,&maximized);POINT origin{};ClientToScreen(window,&origin);MONITORINFO info{};info.cbSize=sizeof(info);
    check(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&info)!=FALSE,"query monitor work area");
    check(origin.x==info.rcWork.left&&origin.y==info.rcWork.top&&maximized.right==info.rcWork.right-info.rcWork.left&&maximized.bottom==info.rcWork.bottom-info.rcWork.top,"maximized client stays within work area");
    client_frame::theme(window,true);client_frame::theme(window,false);DestroyWindow(window);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    return failures?1:0;
}
