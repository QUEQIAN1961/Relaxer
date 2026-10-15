#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "windows-pacman-lock.h"
#include <iostream>

namespace {
std::filesystem::path environment(const wchar_t *name) {
    auto size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size)
        return {};
    std::wstring value(size, L'\0');
    auto count = GetEnvironmentVariableW(name, value.data(), size);
    if (!count || count >= size)
        return {};
    value.resize(count);
    return std::filesystem::path(value).lexically_normal().make_preferred();
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    if (argc != 3 || (std::wstring(argv[1]) != L"--recover-pacman-lock" &&
                      std::wstring(argv[1]) != L"--recover-selected-pacman-lock")) {
        std::cerr << "This is Relaxer's internal database-lock checker. Use RelaxerStart.exe.\n";
        return 2;
    }
    try {
        std::wstring executable(32768, L'\0');
        auto count = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
        if (!count || count == executable.size())
            throw std::runtime_error("Cannot locate helper");
        executable.resize(count);
        auto result = relaxer::launch::pacman_lock::recover(
            std::filesystem::path(executable).parent_path(), environment(L"RELAXER_SOURCE_ROOT"),
            environment(L"RELAXER_BUILD_CACHE"), environment(L"RELAXER_MSYS2_DIR"), argv[2],
            std::wstring(argv[1]) == L"--recover-selected-pacman-lock");
        // UTF-8 matches the Bash capture and preparation log decoder.
        std::cout << relaxer::launch::pacman_lock::diagnostic(result) << '\n';
        return result.code;
    } catch (const std::exception &) {
        std::cerr << "数据库锁检查失败，原锁保留；请提供 prepare-startup.log。\n";
        return 75;
    }
}
