// Small native bootstrap; no Qt, Python or MSYS2 DLL is needed to run it.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "core-update-restart.h"
#include "launch-plan.h"
#include "owned-cache.h"
#include "prepare-flow.h"
#include "prepare-window.h"
#include "windows-log.h"
#include "windows-pacman-lock.h"
#include "windows-process.h"
#include <algorithm>
#include <fstream>
#include <shlobj.h>
#include <vector>
#include <windows.h>

namespace {
namespace fs = std::filesystem;
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
};
std::wstring env(const wchar_t *name) {
    DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size)
        return {};
    std::wstring value(size, L'\0');
    DWORD actual = GetEnvironmentVariableW(name, value.data(), size);
    if (!actual || actual >= size)
        return {};
    value.resize(actual);
    return value;
}
void set(const wchar_t *key, const std::wstring &value) {
    if (!SetEnvironmentVariableW(key, value.empty() ? nullptr : value.c_str()))
        throw std::runtime_error("Cannot set startup environment");
}
std::wstring systemError(DWORD code) {
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    std::wstring result = buffer ? buffer : L"未知系统错误";
    if (buffer)
        LocalFree(buffer);
    return result + L"\n错误码：" + std::to_wstring(code);
}
void fail(const std::wstring &message) {
    MessageBoxW(nullptr, message.c_str(), L"Relaxer 启动提示", MB_OK | MB_ICONERROR);
}
std::wstring logExcerpt(const std::string &bytes) {
    std::wstring result = relaxer::launch::decodeLog(bytes);
    // Compiler and package-manager diagnostics can be long. Show their last
    // twelve nonempty lines, keeping the complete output in the log.
    std::vector<std::wstring> lines;
    size_t start = 0;
    for (size_t i = 0; i <= result.size(); ++i) {
        if (i == result.size() || result[i] == L'\n') {
            auto line = result.substr(start, i - start);
            if (!line.empty() && line.back() == L'\r')
                line.pop_back();
            if (!line.empty())
                lines.push_back(line);
            start = i + 1;
        }
    }
    result.clear();
    size_t first = lines.size() > 12 ? lines.size() - 12 : 0;
    for (size_t i = first; i < lines.size(); ++i)
        result += lines[i] + L"\n";
    if (result.size() > 1400)
        result = result.substr(result.size() - 1400);
    return result;
}
fs::path location() {
    std::vector<wchar_t> buffer(32768);
    DWORD n = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (!n || n >= buffer.size())
        throw std::runtime_error("Cannot locate launcher");
    return fs::path(std::wstring(buffer.data(), n)).parent_path();
}
bool isMsys(const fs::path &p) {
    return !p.empty() && fs::is_regular_file(p / "usr/bin/bash.exe") && fs::is_directory(p / "ucrt64");
}
fs::path findMsys(const fs::path &root) {
    fs::path custom(env(L"RELAXER_MSYS2_DIR"));
    if (!custom.empty()) {
        if (!isMsys(custom))
            throw std::runtime_error("Explicit MSYS2 directory is incomplete");
        return custom;
    }
    // Same saved-path and common-install discovery as Harbor.
    std::ifstream config(root / "msys2-path.txt", std::ios::binary);
    std::string line;
    if (std::getline(config, line)) {
        if (line.size() >= 3 && line.compare(0, 3, "\xef\xbb\xbf") == 0)
            line.erase(0, 3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto saved = fs::u8path(line);
        if (isMsys(saved)) return saved;
    }
    for (auto candidate : {L"C:\\Software\\MSYS2", L"C:\\msys64"})
        if (isMsys(candidate)) return fs::path(candidate);
    // Also recognize a locally installed MSYS2 exposed via PATH. Do not run
    // where.exe or interpret any path as shell code.
    const auto path = env(L"PATH");
    for (size_t begin = 0; begin < path.size();) {
        const auto end = path.find(L';', begin);
        auto part = path.substr(begin, end == std::wstring::npos ? end : end - begin);
        if (part.size() >= 2 && part.front() == L'"' && part.back() == L'"')
            part = part.substr(1, part.size() - 2);
        fs::path candidate(part);
        if (candidate.is_absolute() && GetDriveTypeW(candidate.root_path().c_str()) == DRIVE_FIXED) {
            if (isMsys(candidate)) return candidate;
            if (isMsys(candidate.parent_path())) return candidate.parent_path();
            if (isMsys(candidate.parent_path().parent_path())) return candidate.parent_path().parent_path();
        }
        if (end == std::wstring::npos) break;
        begin = end + 1;
    }
    return {};
}
void environment(const fs::path &root, const fs::path &msys) {
    wchar_t windows[MAX_PATH]{};
    if (!GetWindowsDirectoryW(windows, MAX_PATH))
        throw std::runtime_error("Cannot locate Windows directory");
    auto w = fs::path(windows);
    std::wstring path = root.wstring() + L";" + (root / "bin").wstring();
    if (!msys.empty())
        path += L";" + (msys / "ucrt64/bin").wstring();
    path += L";" + (w / "System32").wstring() + L";" + w.wstring() + L";" + (w / "System32/Wbem").wstring();
    set(L"PATH", path);
    set(L"RELAXER_ROOT", root.wstring());
    set(L"RELAXER_MSYS2_DIR", msys.empty() ? L"" : msys.wstring());
    set(L"QT_MEDIA_BACKEND", L"ffmpeg");
    for (auto key : {L"QT_PLUGIN_PATH", L"QT_QPA_PLATFORM_PLUGIN_PATH", L"QT_QPA_PLATFORM",
                     L"QML_IMPORT_PATH", L"QML2_IMPORT_PATH"})
        set(key, L"");
    if (!msys.empty()) {
        set(L"RELAXER_MSYS2_DIR", msys.wstring());
        for (auto relative : {L"share/qt6/plugins", L"lib/qt6/plugins"}) {
            auto plugins = msys / "ucrt64" / relative;
            if (fs::is_regular_file(plugins / "platforms/qwindows.dll")) {
                set(L"QT_PLUGIN_PATH", plugins.wstring());
                break;
            }
        }
    }
}
fs::path powershell() {
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH))
        throw std::runtime_error("Cannot locate PowerShell");
    return fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
}
bool asciiPath(const fs::path &path) {
    for (wchar_t ch : path.wstring())
        if (ch <= 32 || ch > 126)
            return false;
    return true;
}
fs::path cacheLocation(const fs::path &root) {
    // MSYS2 recommends a short ASCII path on a local disk. Try the user's private
    // locations first; use an NTFS short name when the user directory has spaces.
    std::vector<fs::path> candidates;
    auto custom = env(L"RELAXER_CACHE_DIR");
    if (!custom.empty())
        candidates.emplace_back(custom);
    std::ifstream preference(root / "build-cache-path.txt", std::ios::binary);
    std::string saved;
    if (std::getline(preference, saved) && !saved.empty()) {
        auto path = fs::u8path(saved);
        if (relaxer::launch::ownedCache(path, root))
            return path;
    }
    auto local = env(L"LOCALAPPDATA");
    if (!local.empty())
        candidates.emplace_back(fs::path(local) / L"Relaxer/CppBuildCache");
    wchar_t temp[32768]{};
    if (GetTempPathW(32768, temp))
        candidates.emplace_back(fs::path(temp) / L"RelaxerBuildCache");
    auto publicPath = env(L"PUBLIC");
    if (!publicPath.empty())
        candidates.emplace_back(fs::path(publicPath) / L"Documents/RelaxerBuildCache");
    candidates.emplace_back(root.root_path() / L"RelaxerBuildCache");
    for (auto base : candidates) {
        try {
            fs::create_directories(base);
            if (!asciiPath(base)) {
                wchar_t shortName[32768]{};
                auto size = GetShortPathNameW(base.c_str(), shortName, 32768);
                if (!size || size >= 32768)
                    continue;
                base = shortName;
            }
            if (!asciiPath(base) || base.wstring().size() > 130)
                continue;
            wchar_t fileSystem[32]{};
            const auto volume = base.root_path();
            if (GetDriveTypeW(volume.c_str()) != DRIVE_FIXED ||
                !GetVolumeInformationW(volume.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fileSystem,
                                       32) ||
                _wcsicmp(fileSystem, L"NTFS") != 0)
                continue;
            auto cache = base / relaxer::launch::cacheKey(root);
            relaxer::launch::claimCache(cache, root);
            std::ofstream stored(root / "build-cache-path.txt", std::ios::binary);
            stored << cache.u8string() << '\n';
            return cache;
        } catch (const std::exception &) { /* try another writable, local path */
        }
    }
    throw std::runtime_error("Cannot create a short ASCII build-cache path");
}
fs::path buildDirectory(const fs::path &cache, const fs::path &msys) {
    auto original = cache / "build";
    // A previously locked CMake tree may have required a fresh isolated build.
    // Never trust an absolute path or a path escaping Relaxer's owned cache.
    std::ifstream active(cache / "active-build-directory.txt", std::ios::binary);
    std::string leaf;
    if (std::getline(active, leaf) && leaf.rfind("build-recovery-", 0) == 0 &&
        leaf.size() <= 64 &&
        std::all_of(leaf.begin(), leaf.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        }) && !fs::is_symlink(cache / leaf) && fs::is_directory(cache / leaf)) {
        original = cache / leaf;
    }
    std::ifstream config(original / "CMakeCache.txt", std::ios::binary);
    std::string line;
    while (std::getline(config, line)) {
        if (line.rfind("CMAKE_CXX_COMPILER:", 0) != 0) continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) break;
        auto compiler = line.substr(equal + 1);
        if (!compiler.empty() && compiler.back() == '\r') compiler.pop_back();
        if (!relaxer::launch::pacman_lock::samePath(fs::u8path(compiler).parent_path(), msys / "ucrt64/bin"))
            // Keep the old result for retries, but never combine objects/Qt
            // configuration from one MSYS2 with another one's deployed DLLs.
            return cache / ("build-" + relaxer::launch::cacheKey(msys));
        break;
    }
    return original;
}
void cleanupCache(const fs::path &root, const fs::path &cache,
                  const std::function<void(const char *, DWORD)> &output) {
    if (!fs::is_regular_file(root / "runtime/relaxer-ready.json") ||
        !fs::is_regular_file(root / "runtime/Relaxer.exe"))
        throw std::runtime_error("The standalone runtime has not been verified");
    auto gpg = cache / "msys64/usr/bin/gpgconf.exe";
    if (fs::is_regular_file(gpg)) {
        // Only the pacman keyring inside Relaxer's downloaded environment is stopped.
        // Never use a module-name filter or stop another user's MSYS2 processes.
        auto outcome = relaxer::launch::capture(
            gpg, {L"--homedir", (cache / L"msys64/etc/pacman.d/gnupg").wstring(), L"--kill", L"all"}, root,
            output, 30000);
        (void)outcome;
    }
    relaxer::launch::cleanOwnedCache(cache, root);
}
bool prepare(const fs::path &root, fs::path msys) {
    for (auto script :
         {"scripts/install-windows-dependencies.sh", "scripts/build-windows-ucrt64.sh",
          "scripts/online-msys2.ps1", "scripts/collect-runtime.sh", "scripts/verify-runtime.ps1"}) {
        if (!fs::is_regular_file(root / script)) {
            fail(L"缺少首次准备脚本，请解压完整源码包。\n" + (root / script).wstring());
            return false;
        }
    }
    auto cache = cacheLocation(root);
    if (msys.empty())
        msys = cache / L"msys64";
    set(L"RELAXER_SOURCE_ROOT", root.wstring());
    set(L"RELAXER_BUILD_CACHE", cache.wstring());
    set(L"RELAXER_BUILD_DIR", buildDirectory(cache, msys).wstring());
    set(L"RELAXER_PREP_CAPTURE", L"1");
    set(L"MSYSTEM", L"UCRT64");
    set(L"CHERE_INVOKING", L"1");
    set(L"MSYS2_PATH_TYPE", L"minimal");
    set(L"MSYS2_ENV_CONV_EXCL", L"RELAXER_");
    SYSTEM_INFO processors{};
    GetSystemInfo(&processors);
    unsigned buildJobs = processors.dwNumberOfProcessors;
    if (buildJobs < 1)
        buildJobs = 1;
    if (buildJobs > 256)
        buildJobs = 256;
    set(L"RELAXER_BUILD_JOBS", std::to_wstring(buildJobs));
    auto logPath = root / "prepare-startup.log";
    Handle log(CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!log.valid()) {
        fail(L"无法创建准备日志。\n" + systemError(GetLastError()));
        return false;
    }
    relaxer::launch::PrepareWindow view(logPath);
    return view.run([&](relaxer::launch::PrepareProgress &progress, const std::function<void()> &notify) {
        std::string tail;
        bool logOk = true;
        auto write = [&](const char *bytes, DWORD length) {
            DWORD written = 0;
            if (!WriteFile(log.value, bytes, length, &written, nullptr) || written != length)
                logOk = false;
            progress.feed(bytes, length);
            notify();
            tail.append(bytes, length);
            if (tail.size() > 65536)
                tail.erase(0, tail.size() - 65536);
        };
        auto note = [&](const std::string &line) { write(line.data(), DWORD(line.size())); };
        note("\r\n===== Relaxer first-start preparation (online-portable-v1.2) =====\r\n");
        note("Project directory: " + root.u8string() + "\r\nOwned cache: " + cache.u8string() + "\r\n");
        note("Selected MSYS2: " + msys.u8string() + "\r\n");
        note("Environment selection: " + std::string(relaxer::launch::pacman_lock::samePath(msys, cache / "msys64")
                 ? "Relaxer private cache" : "reused existing installation") + "\r\n");
        note("Build jobs: " + std::to_string(buildJobs) + "\r\n");
        const auto interruptedUpdate = cache / ".relaxer-update-in-progress";
        using S = relaxer::launch::PrepareStage;
        auto stage = S::Check;
        if (relaxer::launch::select(root).kind == relaxer::launch::Kind::Portable &&
            fs::is_regular_file(root / "runtime/relaxer-ready.json")) {
            progress.steps.fill(relaxer::launch::StepState::Skipped);
            progress.steps[relaxer::launch::PrepareProgress::index(S::Verify)] =
                relaxer::launch::StepState::Complete;
            progress.begin(S::Cleanup);
            notify();
            try {
                cleanupCache(root, cache, write);
                progress.complete(S::Cleanup);
                progress.done();
            } catch (const std::exception &) {
                progress.fail(L"程序已通过验证，但缓存仍被占用。请关闭相关进程后重试。\n" + cache.wstring());
            }
            notify();
            return;
        }
        const bool downloadNeeded = relaxer::launch::pacman_lock::samePath(msys, cache / "msys64");
        if (!downloadNeeded)
            progress.steps[relaxer::launch::PrepareProgress::index(S::Download)] =
                relaxer::launch::StepState::Skipped;
        if (fs::is_regular_file(interruptedUpdate))
            note("An earlier update was interrupted; resume the full update before build.\r\n");
        auto result = relaxer::launch::runFirstStart(downloadNeeded, [&](S next) {
            stage = next;
            progress.begin(stage);
            notify();
            note("\r\nStage: " + std::string(relaxer::launch::stageKey(stage)) + "\r\n");
            relaxer::launch::ProcessResult outcome;
            if (stage == S::Download) {
                outcome = relaxer::launch::capture(powershell(),
                                                   {L"-NoLogo", L"-NoProfile", L"-ExecutionPolicy", L"Bypass",
                                                    L"-File", (root / L"scripts/online-msys2.ps1").wstring()},
                                                   root, write);
                if (outcome.started && !outcome.code && !outcome.systemError && !outcome.readError)
                    msys = cache / "msys64";
            } else if (stage == S::Verify) {
                outcome =
                    relaxer::launch::capture(powershell(),
                                             {L"-NoLogo", L"-NoProfile", L"-ExecutionPolicy", L"Bypass",
                                              L"-File", (root / L"scripts/verify-runtime.ps1").wstring()},
                                             root, write);
            } else if (stage == S::Cleanup) {
                outcome.started = true;
                try {
                    cleanupCache(root, cache, write);
                    note("Owned build cache cleaned. Existing MSYS2 installations were preserved.\r\n");
                } catch (const std::exception &error) {
                    outcome.code = 1;
                    note(std::string("Cache cleanup failed: ") + error.what() + "\r\n");
                }
            } else {
                environment(root, msys);
                auto arguments = stage == S::Deploy
                                     ? std::vector<std::wstring>{L"--login", L"scripts/collect-runtime.sh"}
                                     : relaxer::launch::bashArguments(stage);
                const bool transaction = stage == S::UpdateCore || stage == S::UpdateRemaining || stage == S::Install;
                if (stage == S::UpdateCore) {
                    std::ofstream marker(interruptedUpdate, std::ios::binary);
                    marker << "Update must finish before build\n";
                    if (!marker)
                        throw std::runtime_error("Cannot record pending MSYS2 update");
                }
                // Check/recover on Windows, before Bash can rewrite ownership paths.
                // Bash still independently checks the actual pacman DBPath.
                outcome = relaxer::launch::runRestartableUpdate(stage, [&](relaxer::launch::CoreUpdateRestart &coreUpdate) {
                    relaxer::launch::ProcessResult phase;
                    if (transaction) {
                        const auto lock = msys / "var/lib/pacman/db.lck";
                        if (fs::exists(lock)) {
                            note("Native lock check: " + lock.u8string() + "\r\n");
                            auto checked = relaxer::launch::pacman_lock::recover(root, root, cache, msys, lock, true);
                            const auto waitingSince = GetTickCount64();
                            while (checked.waitable && GetTickCount64() - waitingSince < 90000) {
                                note("MSYS2 package manager or database lock is still busy; waiting before reuse.\r\n");
                                for (unsigned tick = 0; tick < 20; ++tick) Sleep(250);
                                checked = relaxer::launch::pacman_lock::recover(root, root, cache, msys, lock, true);
                            }
                            note(relaxer::launch::pacman_lock::diagnostic(checked) + "\r\n");
                            if (checked.code) {
                                phase.started = true;
                                phase.code = checked.code;
                                return phase;
                            }
                        }
                    }
                    return relaxer::launch::capture(msys / "usr/bin/bash.exe", arguments, root,
                        [&](const char *bytes, DWORD length) {
                            if (stage == S::UpdateCore || stage == S::UpdateRemaining)
                                coreUpdate.feed(bytes, length);
                            write(bytes, length);
                        });
                }, note);
                if (stage == S::Install && outcome.started && !outcome.code && !outcome.systemError && !outcome.readError)
                    fs::remove(interruptedUpdate);
            }
            note("Stage result: exit=" + std::to_string(outcome.code) +
                 ", system-error=" + std::to_string(outcome.systemError) +
                 ", read-error=" + std::to_string(outcome.readError) + "\r\n");
            const bool checkedMissing = stage == S::Check && outcome.code == 1;
            if (outcome.started && !outcome.systemError && !outcome.readError &&
                (outcome.code == 0 || checkedMissing))
                progress.complete(stage);
            notify();
            return outcome;
        }, fs::is_regular_file(interruptedUpdate));
        if (!result.started || result.systemError || result.readError || result.code != 0) {
            progress.fail(L"首次准备未完成。阶段：" + std::wstring(relaxer::launch::stageLabel(stage)) +
                          L"\n退出码：" + std::to_wstring(result.code) +
                          L"\n缓存保留，重试可复用已有下载和编译结果。\n\n" + logExcerpt(tail) + L"\n日志：" +
                          logPath.wstring());
            // Cleanup follows commit. A retry of this particular failure only cleans
            // the verified cache, rather than overwriting the committed runtime.
            return;
        }
        if (!logOk || !fs::is_regular_file(root / "runtime/relaxer-ready.json") ||
            !fs::is_regular_file(root / "runtime/Relaxer.exe")) {
            progress.fail(L"准备结束但运行包或完整日志缺失。请提供 prepare-startup.log。");
            return;
        }
        progress.done();
        set(L"RELAXER_PREP_CAPTURE", L"");
    });
}
int run(const fs::path &root, const fs::path &exe) {
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle log(CreateFileW((root / "startup.log").c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr));
    Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!log.valid() || !input.valid()) {
        fail(L"无法写入启动日志。请把完整文件夹解压到桌面等可写位置后再运行。\n\n" + root.wstring());
        return 1;
    }
    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::string heading = "\r\nRelaxer start " + std::to_string(now.wYear) + "-" +
                          std::to_string(now.wMonth) + "-" + std::to_string(now.wDay) + " " +
                          std::to_string(now.wHour) + ":" + std::to_string(now.wMinute) + "\r\n";
    DWORD written = 0;
    WriteFile(log.value, heading.data(), DWORD(heading.size()), &written, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.value;
    startup.hStdOutput = startup.hStdError = log.value;
    PROCESS_INFORMATION process{};
    std::wstring command = relaxer::launch::quote(exe.wstring()) + L" --launch";
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        root.c_str(), &startup, &process)) {
        fail(L"无法打开 Relaxer。\n" + systemError(GetLastError()) +
             L"\n\n不要单独移动 EXE，请保留整个程序文件夹。\n日志：" + (root / "startup.log").wstring());
        return 1;
    }
    Handle thread(process.hThread), child(process.hProcess);
    // Catch missing DLLs and other immediate loader failures without waiting for
    // the user's entire desktop session or creating a console window.
    if (WaitForSingleObject(child.value, 2500) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(child.value, &code);
        if (code != 0) {
            fail(L"Relaxer 在启动时退出了。\n\n退出码：" + std::to_wstring(code) +
                 L"\n请提供 startup.log；若缺少 DLL，请重新部署完整便携包。\n日志：" +
                 (root / "startup.log").wstring());
            return 1;
        }
    }
    return 0;
}
} // namespace
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 1;
    try {
        auto root = location();
        auto plan = relaxer::launch::select(root);
        if (plan.kind == relaxer::launch::Kind::Incomplete) {
            fail(L"没有找到完整的 Relaxer 文件。\n\n请先把 ZIP 全部解压到一个新文件夹，"
                 L"再运行其中的 RelaxerStart.exe。不要直接在压缩包内运行，也不要只取出 EXE。");
        } else {
            Handle guard(CreateFileW((root / "startup.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                     nullptr, OPEN_ALWAYS,
                                     FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
            if (!guard.valid()) {
                fail(L"另一个启动器正在准备或启动 Relaxer，或项目目录不可写。\n"
                     L"请等待准备结束；如文件夹位于只读位置，请完整移到桌面。");
            } else {
                if (plan.kind != relaxer::launch::Kind::Portable) {
                    if (prepare(root, findMsys(root)))
                        plan = relaxer::launch::select(root);
                    else
                        plan.kind = relaxer::launch::Kind::Incomplete;
                }
                if (plan.kind == relaxer::launch::Kind::Portable) {
                    // A previous cleanup failure must be retried without rebuilding or
                    // downloading; an already verified runtime is retained throughout.
                    std::ifstream stored(root / "build-cache-path.txt", std::ios::binary);
                    std::string cached;
                    if (std::getline(stored, cached) && !cached.empty()) {
                        auto path = fs::u8path(cached);
                        if (relaxer::launch::ownedCache(path, root) &&
                            fs::is_regular_file(root / "runtime/relaxer-ready.json")) {
                            try {
                                cleanupCache(root, path, [](const char *, DWORD) {});
                            } catch (const std::exception &) {
                                fail(L"程序已通过独立验证，但临时缓存仍被占用。缓存已保留，下次启动会再次清理"
                                     L"。\n" +
                                     path.wstring());
                            }
                        }
                    }
                    const auto runtime = plan.executable.parent_path();
                    environment(runtime, {});
                    set(L"RELAXER_DATA", (root / L"data").wstring());
                    result = run(runtime, plan.executable);
                }
            }
        }
    } catch (const std::exception &) {
        fail(L"无法读取启动文件或设置启动环境。请完整解压到可写的新文件夹后再试。");
    }
    if (SUCCEEDED(com))
        CoUninitialize();
    return result;
}
