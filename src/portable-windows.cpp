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
#include <algorithm>
#include <atomic>
#include <exception>
#include <thread>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
fs::path extended(const fs::path &file) {
    auto name=fs::absolute(file).lexically_normal().make_preferred().wstring();
    if(name.rfind(L"\\\\?\\",0)==0)return fs::path(name);
    if(name.rfind(L"\\\\",0)==0)return fs::path(L"\\\\?\\UNC\\"+name.substr(2));
    return fs::path(L"\\\\?\\"+name);
}
struct VerificationMetrics {
    double entryQpcMs = now(), manifestMs = 0, parallelVerificationMs = 0;
    double pathChecksWorkMs = 0, sha256WorkMs = 0, verifiedQpcMs = 0;
    double createStartQpcMs = 0, createdQpcMs = 0;
    size_t files = 0, bytes = 0, workers = 0;
    static double now() {
        LARGE_INTEGER value, frequency;
        QueryPerformanceCounter(&value); QueryPerformanceFrequency(&frequency);
        return double(value.QuadPart) * 1000. / double(frequency.QuadPart);
    }
    void write(const fs::path &path) const {
        if (path.empty()) return;
        std::ofstream output(extended(path), std::ios::binary);
        output.precision(15);
        output << "{\"entryQpcMs\":" << entryQpcMs << ",\"manifestMs\":" << manifestMs
            << ",\"parallelVerificationMs\":" << parallelVerificationMs
            << ",\"pathChecksWorkMs\":" << pathChecksWorkMs << ",\"sha256WorkMs\":" << sha256WorkMs
            << ",\"sourceScanMs\":0,\"verifiedQpcMs\":" << verifiedQpcMs
            << ",\"createStartQpcMs\":" << createStartQpcMs << ",\"createdQpcMs\":" << createdQpcMs
            << ",\"files\":" << files << ",\"bytes\":" << bytes << ",\"workers\":" << workers << "}\n";
    }
};
VerificationMetrics metrics;
struct Entry { fs::path relative; std::string expected; bool checkIdentity; };
// One provider and read buffer per temporary worker. No persistent cache or
// metadata shortcut: every manifest file is read and SHA-256 checked each run.
class Hasher {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    std::array<char, 65536> buffer;
public:
    Hasher() {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            throw std::runtime_error("Cannot initialize SHA256 verification");
    }
    ~Hasher() { if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    void verify(const fs::path &file, const Entry &entry, size_t &byteCount) {
        std::ifstream input(extended(file), std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read a portable runtime file");
        BCRYPT_HASH_HANDLE hash = nullptr;
        struct Cleanup { BCRYPT_HASH_HANDLE &hash; ~Cleanup() { if (hash) BCryptDestroyHash(hash); } } cleanup{hash};
        if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
            throw std::runtime_error("Cannot initialize SHA256 verification");
        const std::string identity = std::string("RelaxerBuildSourceSHA256=") + RELAXER_BUILD_SOURCE_SHA256 + '\0';
        std::string tail;
        bool identityFound = false;
        while (input) {
            input.read(buffer.data(), buffer.size());
            const auto count = size_t(input.gcount());
            byteCount += count;
            if (count && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(count), 0) < 0)
                throw std::runtime_error("Cannot hash a portable runtime file");
            if (entry.checkIdentity && !identityFound) {
                tail.append(buffer.data(), count);
                identityFound = tail.find(identity) != std::string::npos;
                if (tail.size() >= identity.size()) tail.erase(0, tail.size() - identity.size() + 1);
            }
        }
        if (!input.eof()) throw std::runtime_error("Incomplete runtime read");
        std::array<unsigned char, 32> bytes{};
        if (BCryptFinishHash(hash, bytes.data(), ULONG(bytes.size()), 0) < 0)
            throw std::runtime_error("Cannot finish SHA256 verification");
        std::string actual;
        actual.reserve(64);
        for (const auto byte : bytes) { actual += "0123456789abcdef"[byte >> 4]; actual += "0123456789abcdef"[byte & 15]; }
        if (actual != entry.expected) throw std::runtime_error("Portable runtime checksum mismatch");
        if (entry.checkIdentity && !identityFound) throw std::runtime_error("Portable application source identity mismatch");
    }
};
void verify(const fs::path &root) {
    const auto manifestStart = VerificationMetrics::now();
    std::ifstream manifest(extended(root / "PORTABLE-SHA256SUMS.txt"), std::ios::binary);
    if (!manifest) throw std::runtime_error("Missing PORTABLE-SHA256SUMS.txt");
    std::map<std::string, std::string> names;
    std::vector<Entry> entries;
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
        if (!names.emplace(name, line.substr(0, 64)).second)
            throw std::runtime_error("Duplicate portable checksum entry");
        entries.push_back({relative, line.substr(0, 64), name == "runtime/Relaxer.exe" || name == "runtime/RelaxerQmlUi.dll"});
    }
    if (!manifest.eof()) throw std::runtime_error("Incomplete checksum manifest");
    for (const auto *name : {"runtime/Relaxer.exe", "runtime/RelaxerQmlUi.dll", "runtime/Qt6Core.dll",
                             "runtime/Qt6Quick.dll", "runtime/platforms/qwindows.dll", "runtime/qt.conf"})
        if (!names.count(name)) throw std::runtime_error("Incomplete portable checksum manifest");
    metrics.manifestMs = VerificationMetrics::now() - manifestStart;
    const auto verificationStart = VerificationMetrics::now();
    constexpr size_t maxWorkers = 4;
    const size_t workers = std::min(maxWorkers, entries.size());
    struct Work { size_t files = 0, bytes = 0; double paths = 0, hashes = 0; std::exception_ptr error; };
    std::array<Work, maxWorkers> results{};
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(workers);
    try {
        for (size_t worker = 0; worker < workers; ++worker) threads.emplace_back([&, worker] {
            auto &result = results[worker];
            try {
                Hasher hasher;
                while (!failed.load(std::memory_order_relaxed)) {
                    const size_t index = next.fetch_add(1, std::memory_order_relaxed);
                    if (index >= entries.size()) break;
                    const auto &entry = entries[index];
                    const auto pathStart = VerificationMetrics::now();
                    // Retain RC6's per-file, per-component reparse checks.
                    auto current = root;
                    for (const auto &part : entry.relative) {
                        current /= part;
                        const auto attributes = GetFileAttributesW(extended(current).c_str());
                        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                            throw std::runtime_error("Missing runtime file or reparse point");
                    }
                    result.paths += VerificationMetrics::now() - pathStart;
                    const auto hashStart = VerificationMetrics::now();
                    hasher.verify(current, entry, result.bytes);
                    result.hashes += VerificationMetrics::now() - hashStart;
                    ++result.files;
                }
            } catch (...) { result.error = std::current_exception(); failed.store(true, std::memory_order_relaxed); }
        });
    } catch (...) { for (auto &thread : threads) thread.join(); throw; }
    for (auto &thread : threads) thread.join();
    for (const auto &result : results) {
        if (result.error) std::rethrow_exception(result.error);
        metrics.files += result.files; metrics.bytes += result.bytes;
        metrics.pathChecksWorkMs += result.paths; metrics.sha256WorkMs += result.hashes;
    }
    if (metrics.files != entries.size()) throw std::runtime_error("Incomplete portable verification");
    metrics.workers = workers;
    metrics.parallelVerificationMs = VerificationMetrics::now() - verificationStart;
    metrics.verifiedQpcMs = VerificationMetrics::now();
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
        fs::path timingOutput;
        for (size_t i=0; i<input.size(); ++i) {
            if (input[i]==L"--diagnostics" && i+1<input.size()) timingOutput=input[i+1]+L".launcher.json";
            else if (input[i].rfind(L"--diagnostics=",0)==0) timingOutput=input[i].substr(14)+L".launcher.json";
        }
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
        metrics.createStartQpcMs=VerificationMetrics::now();
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, runtime.c_str(), &startup, &process))
            throw std::runtime_error("Cannot start the portable application");
        metrics.createdQpcMs=VerificationMetrics::now();
        metrics.write(timingOutput);
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
