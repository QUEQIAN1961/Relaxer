#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "prepare-window.h"
#include <gdiplus.h>
#include <iostream>
using namespace relaxer::launch;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static void captureOwnWindow(const fs::path &path){
    auto hwnd=FindWindowW(L"RelaxerPreparationWindowV14",nullptr);DWORD process=0;
    GetWindowThreadProcessId(hwnd,&process);require(hwnd&&process==GetCurrentProcessId(),"preview window ownership");
    RECT bounds{};GetWindowRect(hwnd,&bounds);const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    auto screen=GetDC(hwnd);auto dc=CreateCompatibleDC(screen);auto bitmap=CreateCompatibleBitmap(screen,width,height);
    auto old=SelectObject(dc,bitmap);const bool printed=PrintWindow(hwnd,dc,2);SelectObject(dc,old);
    ReleaseDC(hwnd,screen);DeleteDC(dc);require(printed,"native preview capture");
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
    require(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok,"PNG encoder startup");
    bool saved=false;
    {Gdiplus::Bitmap image(bitmap,nullptr);const CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    saved=image.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok;}
    Gdiplus::GdiplusShutdown(token);DeleteObject(bitmap);require(saved,"native preview save");
}
int main(){
    try{
        PrepareProgress progress;progress.begin(PrepareStage::Build);
        const std::string output="RELAXER_PHASE:compile\r\n[111/111] Linking Relaxer.exe\r\nRELAXER_PHASE:test\r\nRELAXER_TEST_TOTAL:20\r\n 1/20 Test #1: relaxer_core .... Passed 0.1 sec\r\n Start 2: relaxer_gui\r\n";
        for(char byte:output)progress.feed(&byte,1);
        require(progress.activeRow()==2&&progress.testsDone==1&&progress.testsTotal==20,"split test output count");
        require(progress.rowState(1)==StepState::Complete&&progress.rowState(2)==StepState::Running,"separate test stage");
        require(progress.statusText(2)==L"1 / 20"&&progress.detail.find(L"relaxer_gui")!=std::wstring::npos,"test label and name");
        progress.line("20/20 Test #20: relaxer_media_smoke ... Passed 1 sec");
        require(progress.percent==74&&progress.testsDone==20,"test fraction advances progress");
        progress.line("RELAXER_PHASE:ffmpeg");require(progress.rowState(2)==StepState::Complete&&progress.activeRow()==3,"test to FFmpeg transition");
        PrepareProgress failure;failure.begin(PrepareStage::Build);failure.line("RELAXER_PHASE:test");failure.fail(L"test exit 8");
        require(failure.rowState(2)==StepState::Failed&&!failure.success,"test failure blocks success");
        PrepareProgress malformed;malformed.begin(PrepareStage::Build);malformed.line("RELAXER_PHASE:test");
        for(auto line:{"RELAXER_TEST_TOTAL:-1","RELAXER_TEST_TOTAL:0","RELAXER_TEST_TOTAL:90000000000","40/20 Test","-1/20 Test"})malformed.line(line);
        require(malformed.testsTotal==0&&malformed.testsDone==0,"invalid fractions ignored");
        const auto dark=PreparationColors::forScheme(true),light=PreparationColors::forScheme(false);
        require(GetRValue(dark.background)<64&&GetRValue(dark.text)>220,"dark preparation palette");
        require(GetRValue(light.background)>240&&GetRValue(light.text)<64,"light preparation palette");
        std::cout<<"PASS: split output, test count/name, separate phases, failed tests, invalid counts, dark/light palettes\n";
        wchar_t directory[32768]{};
        if(GetEnvironmentVariableW(L"RELAXER_QA_DIR",directory,32768)){
            fs::create_directories(directory);
            for(bool scheme:{false,true}){
                PrepareWindow window(fs::path(directory)/"prepare-preview.log",[scheme]{return scheme;});
                require(window.run([&](PrepareProgress &state,const std::function<void()> &notify){
                    state.steps.fill(StepState::Skipped);state.begin(PrepareStage::Build);state.line("RELAXER_PHASE:test");state.line("RELAXER_TEST_TOTAL:20");
                    state.line("17/20 Test #17: relaxer_v110 ... Passed 0.1 sec");state.line(" Start 18: relaxer_phase03");notify();Sleep(350);
                    captureOwnWindow(fs::path(directory)/(scheme?"prepare-dark.png":"prepare-light.png"));state.done();
                }),"preparation preview completion");
            }
            std::cout<<"PASS: native dark and light window previews\n";
        }
        return 0;
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
