// Direct portable application entry. This never prepares source, downloads
// dependencies, or reads/writes a deployment readiness marker.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <bcrypt.h>
#include "launch-arguments.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
fs::path extended(const fs::path &file) {
    auto name=fs::absolute(file).lexically_normal().make_preferred().wstring();
    if(name.rfind(L"\\\\?\\",0)==0)return fs::path(name);
    if(name.rfind(L"\\\\",0)==0)return fs::path(L"\\\\?\\UNC\\"+name.substr(2));
    return fs::path(L"\\\\?\\"+name);
}
std::string digest(const fs::path &file) {
    std::ifstream input(extended(file), std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read a portable runtime file");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    struct Cleanup {
        BCRYPT_ALG_HANDLE &algorithm; BCRYPT_HASH_HANDLE &hash;
        ~Cleanup() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } cleanup{algorithm, hash};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
        throw std::runtime_error("Cannot initialize SHA256 verification");
    std::array<char, 65536> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(input.gcount()), 0) < 0)
            throw std::runtime_error("Cannot hash a portable runtime file");
    }
    if (!input.eof()) throw std::runtime_error("Incomplete runtime read");
    std::array<unsigned char, 32> bytes{};
    if (BCryptFinishHash(hash, bytes.data(), ULONG(bytes.size()), 0) < 0)
        throw std::runtime_error("Cannot finish SHA256 verification");
    std::string result;
    for (auto byte : bytes) { result += "0123456789abcdef"[byte >> 4]; result += "0123456789abcdef"[byte & 15]; }
    return result;
}
void verify(const fs::path &root) {
    std::ifstream manifest(extended(root / "PORTABLE-SHA256SUMS.txt"), std::ios::binary);
    if (!manifest) throw std::runtime_error("Missing PORTABLE-SHA256SUMS.txt");
    std::map<std::string, std::string> entries;
    std::string line;
    while (std::getline(manifest, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 67 || line.substr(64, 2) != "  " ||
            line.substr(0, 64).find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("Invalid portable checksum entry");
        const auto name = line.substr(66);
        if (name.rfind("runtime/", 0) != 0 || name.find_first_of("\\:\t\r\n") != std::string::npos)
            throw std::runtime_error("Invalid portable runtime path");
        const auto relative = fs::u8path(name);
        for (const auto &part : relative)
            if (part == ".." || part == "." || part.empty()) throw std::runtime_error("Unsafe portable runtime path");
        const auto target = root / relative;
        // Reject links/junctions at every component before opening the target.
        auto current = root;
        for (const auto &part : relative) {
            current /= part;
            const auto attributes = GetFileAttributesW(extended(current).c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::runtime_error("Missing runtime file or reparse point");
        }
        if (!entries.emplace(name, line.substr(0, 64)).second || digest(target) != line.substr(0, 64))
            throw std::runtime_error("Portable runtime checksum mismatch");
    }
    if (!manifest.eof()) throw std::runtime_error("Incomplete checksum manifest");
    for (const auto *name : {"runtime/Relaxer.exe", "runtime/RelaxerQmlUi.dll", "runtime/Qt6Core.dll",
                             "runtime/Qt6Quick.dll", "runtime/platforms/qwindows.dll", "runtime/qt.conf"})
        if (!entries.count(name)) throw std::runtime_error("Incomplete portable checksum manifest");
    const std::string expected = std::string("RelaxerBuildSourceSHA256=") + RELAXER_BUILD_SOURCE_SHA256 + '\0';
    for (const auto *name : {"runtime/Relaxer.exe", "runtime/RelaxerQmlUi.dll"}) {
        std::ifstream file(extended(root / name), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(file), {}};
        if (bytes.find(expected) == std::string::npos) throw std::runtime_error("Portable application source identity mismatch");
    }
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    bool diagnostics = false;
    try {
        int count = 0;
        auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!raw) return 64;
        const std::vector<std::wstring> input(raw + 1, raw + count);
        LocalFree(raw);
        const auto args = relaxer::launch::arguments(input);
        diagnostics = args.diagnostics;
        if (!args.valid) return 64;
        std::vector<wchar_t> location(32768);
        const auto size = GetModuleFileNameW(nullptr, location.data(), DWORD(location.size()));
        if (!size || size >= location.size()) throw std::runtime_error("Cannot locate portable application");
        const auto root = fs::path(std::wstring(location.data(), size)).parent_path();
        verify(root);
        wchar_t windows[MAX_PATH]{};
        if (!GetWindowsDirectoryW(windows, MAX_PATH)) throw std::runtime_error("Cannot locate Windows");
        const auto runtime = root / "runtime";
        const auto path = runtime.wstring() + L";" + (runtime / "bin").wstring() + L";" +
                          (fs::path(windows) / "System32").wstring() + L";" + windows;
        if (!SetEnvironmentVariableW(L"PATH", path.c_str())) throw std::runtime_error("Cannot set runtime PATH");
        for (const auto *key : {L"QT_PLUGIN_PATH", L"QT_QPA_PLATFORM_PLUGIN_PATH", L"QT_QPA_PLATFORM",
                               L"QML_IMPORT_PATH", L"QML2_IMPORT_PATH", L"RELAXER_ROOT", L"RELAXER_MSYS2_DIR"})
            SetEnvironmentVariableW(key, nullptr);
        SetEnvironmentVariableW(L"QT_MEDIA_BACKEND", L"ffmpeg");
        const auto exe = runtime / "Relaxer.exe";
        auto command = relaxer::launch::commandLine(exe, args.runtime);
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, runtime.c_str(), &startup, &process))
            throw std::runtime_error("Cannot start the portable application");
        CloseHandle(process.hThread);
        const auto waited = WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 1;
        if (waited == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hProcess);
        if (code && code != 3 && !diagnostics)
            MessageBoxW(nullptr, L"应用启动失败。请重新解压完整私人测试包，或使用 Relaxer.exe --widgets。",
                        L"Relaxer 便携应用", MB_OK | MB_ICONERROR);
        return int(code);
    } catch (const std::exception &error) {
        if(diagnostics)std::fprintf(stderr,"Relaxer portable: %s\n",error.what());
        if (!diagnostics)
            MessageBoxW(nullptr, L"便携应用文件缺失、校验不一致或来源版本不匹配。\n请重新解压完整测试 ZIP，"
                        L"并保留 PORTABLE-SHA256SUMS.txt 与 runtime 文件夹。\n本入口不执行源码准备。",
                        L"Relaxer 便携应用", MB_OK | MB_ICONERROR);
        return 12;
    }
}
