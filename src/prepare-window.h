#pragma once
#include "launch-plan.h"
#include "prepare-progress.h"
#include "prepare-theme.h"
#include <atomic>
#include <commctrl.h>
#include <functional>
#include <mutex>
#include <shellapi.h>
#include <stdexcept>
#include <thread>
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

namespace relaxer::launch {
// Native controls keep first-start preparation independent of Qt / Python.
class PrepareWindow {
    static constexpr UINT updateMessage = WM_APP + 31;
    static constexpr int logId = 201, retryId = 202, closeId = 203, errorId = 204;
    HWND window = nullptr, heading = nullptr, subtitle = nullptr, bar = nullptr;
    HWND detail = nullptr, elapsed = nullptr, logButton = nullptr, retryButton = nullptr;
    HWND closeButton = nullptr, errorButton = nullptr;
    std::array<HWND, PrepareProgress::rowCount> names{}, states{};
    HFONT font = nullptr, titleFont = nullptr;
    HBRUSH background = nullptr;
    bool dark = false;
    PreparationColors colors = PreparationColors::forScheme(false);
    std::function<bool()> darkPreference;
    int dpi = 96;
    bool marquee = false;
    std::atomic<bool> queued{false};
    std::mutex stateMutex;
    PrepareProgress state;
    std::thread worker;
    std::function<void(PrepareProgress &, const std::function<void()> &)> work;
    fs::path logPath;
    ULONGLONG startedAt = 0;
    int scaled(int value) const { return MulDiv(value, dpi, 96); }
    void applySystemTheme() {
        dark = darkPreference();
        colors = PreparationColors::forScheme(dark);
        if (background) DeleteObject(background);
        background = CreateSolidBrush(colors.background);
        if (!window) return;
        const BOOL useDark = dark;
        if (FAILED(DwmSetWindowAttribute(window, 20, &useDark, sizeof(useDark))))
            DwmSetWindowAttribute(window, 19, &useDark, sizeof(useDark));
        if (bar) {
            SetWindowTheme(bar, L"", L"");
            SendMessageW(bar, PBM_SETBKCOLOR, 0, colors.track);
            SendMessageW(bar, PBM_SETBARCOLOR, 0, colors.accent);
        }
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    HWND control(const wchar_t *type, const wchar_t *text, DWORD style, int id = 0) {
        auto child =
            CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, window,
                            reinterpret_cast<HMENU>(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
        if (!child)
            throw std::runtime_error("Cannot create preparation control");
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return child;
    }
    void move(HWND child, int x, int y, int width, int height) {
        MoveWindow(child, scaled(x), scaled(y), scaled(width), scaled(height), TRUE);
    }
    void layout() {
        const auto current = snapshot();
        const int active = current.activeRow();
        move(heading, 24, 16, 552, 30);
        move(subtitle, 24, 48, 552, 42);
        int y = 100;
        for (int i = 0; i < PrepareProgress::rowCount; ++i) {
            move(names[i], 24, y, 390, 24);
            move(states[i], 414, y, 162, 24);
            y += 26;
            if (i == active) {
                move(detail, 46, y, 530, 40);
                y += 44;
            }
        }
        if (active < 0)
            move(detail, 24, y, 552, 40);
        y += active < 0 ? 48 : 8;
        move(bar, 24, y, 552, 18);
        y += 28;
        move(elapsed, 24, y, 552, 20);
        y += 34;
        move(logButton, 24, y, 96, 32);
        move(errorButton, 130, y, 104, 32);
        move(retryButton, 350, y, 104, 32);
        move(closeButton, 466, y, 110, 32);
    }
    void createControls() {
        font = CreateFontW(-scaled(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                           L"Microsoft YaHei UI");
        titleFont = CreateFontW(-scaled(22), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                L"Microsoft YaHei UI");
        if (!font || !titleFont)
            throw std::runtime_error("Cannot create preparation font");
        heading = control(L"STATIC", L"Relaxer C++ 首次准备", 0);
        SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont), TRUE);
        subtitle = control(L"STATIC",
                           L"准备完成后会自动打开程序。\n正在进行的步骤下方显示当前细则，完整输出保存在日志中。",
                           SS_LEFT | SS_NOPREFIX);
        for (int i = 0; i < PrepareProgress::rowCount; ++i) {
            const auto caption = std::to_wstring(i + 1) + L". " + PrepareProgress::rowLabel(i);
            names[i] = control(L"STATIC", caption.c_str(), SS_LEFT | SS_NOPREFIX);
            states[i] = control(L"STATIC", L"等待", SS_RIGHT | SS_NOPREFIX);
        }
        bar = control(PROGRESS_CLASSW, L"", PBS_SMOOTH);
        SendMessageW(bar, PBM_SETRANGE32, 0, 100);
        detail = control(L"STATIC", L"正在开始准备…", SS_LEFT | SS_NOPREFIX | SS_EDITCONTROL);
        elapsed = control(L"STATIC", L"", 0);
        logButton = control(L"BUTTON", L"打开日志", BS_OWNERDRAW | WS_TABSTOP, logId);
        errorButton = control(L"BUTTON", L"错误详情", BS_OWNERDRAW | WS_TABSTOP, errorId);
        retryButton = control(L"BUTTON", L"重试", BS_OWNERDRAW | WS_TABSTOP, retryId);
        closeButton = control(L"BUTTON", L"关闭", BS_OWNERDRAW | WS_TABSTOP, closeId);
        ShowWindow(errorButton, SW_HIDE);
        ShowWindow(retryButton, SW_HIDE);
        applySystemTheme();
        layout();
    }
    PrepareProgress snapshot() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return state;
    }
    void publish(const PrepareProgress &value) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            state = value;
        }
        if (!queued.exchange(true)) {
            if (!PostMessageW(window, updateMessage, 0, 0))
                queued = false;
        }
    }
    void refresh() {
        const auto current = snapshot();
        SetWindowTextW(heading, current.finished
                                    ? (current.success ? L"Relaxer C++ 已准备好" : L"Relaxer C++ 准备未完成")
                                    : L"Relaxer C++ 首次准备");
        for (int i = 0; i < PrepareProgress::rowCount; ++i) {
            SetWindowTextW(states[i], current.statusText(i).c_str());
            InvalidateRect(states[i], nullptr, TRUE);
        }
        const bool animate = !current.determinate && !current.finished;
        if (animate != marquee) {
            auto style = GetWindowLongPtrW(bar, GWL_STYLE);
            SendMessageW(bar, PBM_SETMARQUEE, FALSE, 0);
            SetWindowLongPtrW(bar, GWL_STYLE, animate ? style | PBS_MARQUEE : style & ~LONG_PTR(PBS_MARQUEE));
            SetWindowPos(bar, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            marquee = animate;
            if (animate)
                SendMessageW(bar, PBM_SETMARQUEE, TRUE, 45);
        }
        if (!marquee)
            SendMessageW(bar, PBM_SETPOS, current.percent, 0);
        SetWindowTextW(detail, current.detail.c_str());
        layout();
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        EnableWindow(closeButton, current.finished);
        EnableMenuItem(GetSystemMenu(window, FALSE), SC_CLOSE,
                       MF_BYCOMMAND | (current.finished ? MF_ENABLED : MF_GRAYED));
        ShowWindow(errorButton, current.finished && !current.success ? SW_SHOW : SW_HIDE);
        ShowWindow(retryButton, current.finished && !current.success ? SW_SHOW : SW_HIDE);
        if (current.finished && current.success)
            SetTimer(window, 2, 600, nullptr);
    }
    void startWorker() {
        if (worker.joinable())
            worker.join();
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            state = PrepareProgress{};
        }
        startedAt = GetTickCount64();
        refresh();
        SetTimer(window, 1, 500, nullptr);
        worker = std::thread([this] {
            PrepareProgress progress;
            const auto notify = [&] { publish(progress); };
            try {
                work(progress, notify);
                if (!progress.finished)
                    progress.fail(L"准备流程意外结束，请查看日志。");
            } catch (...) {
                progress.fail(L"无法完成准备进程。请检查磁盘空间、目录权限和准备日志。");
            }
            publish(progress);
        });
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
        auto self = reinterpret_cast<PrepareWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<PrepareWindow *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            self->window = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self)
            return DefWindowProcW(hwnd, message, wp, lp);
        switch (message) {
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
            self->applySystemTheme();
            return 0;
        case WM_ERASEBKGND: {
            RECT bounds{}; GetClientRect(hwnd, &bounds);
            FillRect(reinterpret_cast<HDC>(wp), &bounds, self->background);
            return 1;
        }
        case WM_DRAWITEM: {
            auto item = reinterpret_cast<DRAWITEMSTRUCT *>(lp);
            if (item->CtlType != ODT_BUTTON) break;
            FillRect(item->hDC, &item->rcItem, self->background);
            auto brush = CreateSolidBrush((item->itemState & ODS_SELECTED) ? self->colors.track : self->colors.button);
            auto pen = CreatePen(PS_SOLID, 1, self->colors.border);
            auto oldBrush = SelectObject(item->hDC, brush);
            auto oldPen = SelectObject(item->hDC, pen);
            RoundRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right,
                      item->rcItem.bottom, self->scaled(8), self->scaled(8));
            SelectObject(item->hDC, oldBrush); SelectObject(item->hDC, oldPen);
            DeleteObject(brush); DeleteObject(pen);
            wchar_t text[80]{}; GetWindowTextW(item->hwndItem, text, 80);
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, (item->itemState & ODS_DISABLED) ? self->colors.muted : self->colors.text);
            auto oldFont = SelectObject(item->hDC, self->font);
            auto bounds = item->rcItem;
            DrawTextW(item->hDC, text, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(item->hDC, oldFont);
            if (item->itemState & ODS_FOCUS) { InflateRect(&bounds, -4, -4); DrawFocusRect(item->hDC, &bounds); }
            return TRUE;
        }
        case updateMessage:
            self->queued = false;
            self->refresh();
            return 0;
        case WM_TIMER:
            if (wp == 2) {
                KillTimer(hwnd, 2);
                DestroyWindow(hwnd);
                return 0;
            }
            if (wp == 1) {
                auto value = self->snapshot();
                if (value.finished)
                    KillTimer(hwnd, 1);
                auto seconds = (GetTickCount64() - self->startedAt) / 1000;
                const auto text = L"已用时 " + std::to_wstring(seconds / 60) + L" 分 " +
                                  std::to_wstring(seconds % 60) + L" 秒";
                SetWindowTextW(self->elapsed, text.c_str());
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == logId) {
                wchar_t system[MAX_PATH]{};
                GetSystemDirectoryW(system, MAX_PATH);
                auto editor = fs::path(system) / L"notepad.exe";
                const auto argument = quote(self->logPath.wstring());
                if (reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd, L"open", editor.c_str(), argument.c_str(),
                                                            nullptr, SW_SHOWNORMAL)) <= 32)
                    MessageBoxW(hwnd, self->logPath.c_str(), L"日志位置", MB_OK | MB_ICONINFORMATION);
            } else if (LOWORD(wp) == retryId && self->snapshot().finished)
                self->startWorker();
            else if (LOWORD(wp) == errorId) {
                const auto current = self->snapshot();
                MessageBoxW(hwnd, current.error.c_str(), L"首次准备错误详情", MB_OK | MB_ICONERROR);
            } else if (LOWORD(wp) == closeId)
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case WM_CLOSE:
            if (self->snapshot().finished)
                DestroyWindow(hwnd);
            return 0;
        case WM_CTLCOLORSTATIC: {
            auto dc = reinterpret_cast<HDC>(wp);
            SetBkMode(dc, TRANSPARENT);
            COLORREF color = self->colors.text;
            if (reinterpret_cast<HWND>(lp) == self->subtitle || reinterpret_cast<HWND>(lp) == self->elapsed)
                color = self->colors.muted;
            if (reinterpret_cast<HWND>(lp) == self->detail)
                color = self->snapshot().activeRow() >= 0 ? self->colors.accent : self->colors.muted;
            const auto current = self->snapshot();
            for (int i = 0; i < PrepareProgress::rowCount; ++i)
                if (reinterpret_cast<HWND>(lp) == self->states[i]) {
                    if (i == 3 && current.ffmpegSoft && current.rowState(3) != StepState::Running &&
                        current.rowState(3) != StepState::Failed) {
                        color = self->colors.warning;
                        break;
                    }
                    switch (current.rowState(i)) {
                    case StepState::Running:
                        color = self->colors.accent;
                        break;
                    case StepState::Complete:
                        color = self->colors.complete;
                        break;
                    case StepState::Failed:
                        color = self->colors.failed;
                        break;
                    default:
                        color = self->colors.muted;
                        break;
                    }
                }
            SetTextColor(dc, color);
            return reinterpret_cast<LRESULT>(self->background);
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            KillTimer(hwnd, 2);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }

  public:
    explicit PrepareWindow(fs::path log, std::function<bool()> scheme = preparationSystemDark)
        : darkPreference(std::move(scheme)), logPath(std::move(log)) {}
    ~PrepareWindow() {
        if (worker.joinable())
            worker.join();
        UnregisterClassW(L"RelaxerPreparationWindowV14", GetModuleHandleW(nullptr));
        if (font)
            DeleteObject(font);
        if (titleFont)
            DeleteObject(titleFont);
        if (background)
            DeleteObject(background);
    }
    bool run(const std::function<void(PrepareProgress &, const std::function<void()> &)> &job) {
        work = job;
        applySystemTheme();
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
        if (!InitCommonControlsEx(&controls))
            throw std::runtime_error("Cannot initialize progress controls");
        WNDCLASSW cls{};
        cls.lpfnWndProc = procedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.hbrBackground = nullptr;
        cls.hIcon = LoadIconW(cls.hInstance, L"IDI_ICON1");
        cls.lpszClassName = L"RelaxerPreparationWindowV14";
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register preparation window");
        auto screen = GetDC(nullptr);
        if (screen) {
            dpi = GetDeviceCaps(screen, LOGPIXELSX);
            ReleaseDC(nullptr, screen);
        }
        const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT bounds{0, 0, scaled(600), scaled(520)};
        AdjustWindowRect(&bounds, style, FALSE);
        const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor);
        const int x = monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2;
        const int y = monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2;
        window = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"Relaxer C++ · 首次准备", style, x, y,
                                 width, height, nullptr, nullptr, cls.hInstance, this);
        if (!window)
            throw std::runtime_error("Cannot create preparation window");
        createControls();
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
        startWorker();
        MSG message{};
        int received;
        while ((received = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            if (!IsDialogMessageW(window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (worker.joinable())
            worker.join();
        window = nullptr;
        return received >= 0 && snapshot().success;
    }
};
} // namespace relaxer::launch
