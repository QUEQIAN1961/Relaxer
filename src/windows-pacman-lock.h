#pragma once
#include "owned-cache.h"
#include "readonly-lock-recovery.h"
#include <sstream>
#include <tlhelp32.h>
#include <vector>

namespace relaxer::launch {
// This helper runs before pacman's write transaction, never from --check.
// An existing sentinel is held exclusively while processes are inspected.
namespace pacman_lock {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE handle) : value(handle) {}
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
};
struct Result {
    int code;
    const char *message;
    bool waitable = false;
    DWORD systemError = 0, restoreError = 0, originalAttributes = INVALID_FILE_ATTRIBUTES;
    bool readOnlyCleared = false, readOnlyRestored = false;
};
inline std::string errorText(DWORD code) {
    if (!code) return "未提供系统错误码 / 不适用";
    wchar_t *buffer = nullptr;
    const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    if (!length || !buffer) return "无法取得系统错误文本";
    std::wstring value(buffer, length);
    LocalFree(buffer);
    while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n' || value.back() == L' '))
        value.pop_back();
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0, nullptr, nullptr);
    if (!size) return "无法转换系统错误文本";
    std::string text(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), text.data(), size, nullptr, nullptr);
    return text;
}
inline std::string diagnostic(const Result &result) {
    std::ostringstream out;
    out << result.message;
    if (result.originalAttributes != INVALID_FILE_ATTRIBUTES)
        out << "\nLock attributes: 0x" << std::hex << result.originalAttributes << std::dec;
    if (result.readOnlyCleared)
        out << "\nReadOnly cleared on inspected lock";
    if (result.code) {
        out << "\nWindows error: " << result.systemError << " (" << errorText(result.systemError) << ')';
        if (result.readOnlyCleared) {
            out << "\nOriginal attributes restored: " << (result.readOnlyRestored ? "yes" : "no");
            if (!result.readOnlyRestored)
                out << "; restore error: " << result.restoreError << " (" << errorText(result.restoreError) << ')';
        }
    }
    return out.str();
}
struct WindowsLockOps {
    HANDLE handle;
    FILE_BASIC_INFO original;
    DWORD error = 0;
    bool setAttributes(std::uint32_t attributes) {
        auto update = original;
        update.FileAttributes = attributes;
        const auto ok = SetFileInformationByHandle(handle, FileBasicInfo, &update, sizeof(update));
        error = ok ? ERROR_SUCCESS : GetLastError();
        return ok;
    }
    bool readAttributes(std::uint32_t &attributes) {
        FILE_BASIC_INFO value{};
        const auto ok = GetFileInformationByHandleEx(handle, FileBasicInfo, &value, sizeof(value));
        error = ok ? ERROR_SUCCESS : GetLastError();
        if (ok) attributes = value.FileAttributes;
        return ok;
    }
    bool erase() {
        FILE_DISPOSITION_INFO value{TRUE};
        const auto ok = SetFileInformationByHandle(handle, FileDispositionInfo, &value, sizeof(value));
        error = ok ? ERROR_SUCCESS : GetLastError();
        return ok;
    }
    DWORD lastError() const { return error; }
};
inline bool samePath(std::filesystem::path a, std::filesystem::path b) {
    a = a.lexically_normal().make_preferred();
    b = b.lexically_normal().make_preferred();
    return a.is_absolute() && b.is_absolute() && _wcsicmp(a.c_str(), b.c_str()) == 0;
}
inline bool ordinary(HANDLE handle, bool directory, bool singleLink = true) {
    BY_HANDLE_FILE_INFORMATION info{};
    return GetFileInformationByHandle(handle, &info) &&
           !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
           bool(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == directory &&
           (directory || !singleLink || info.nNumberOfLinks == 1);
}
inline Result recover(const std::filesystem::path &root, const std::filesystem::path &project,
                      const std::filesystem::path &cache, const std::filesystem::path &msys,
                      const std::filesystem::path &lock, bool selectedEnvironment = false) {
    namespace fs = std::filesystem;
    if (!samePath(root, project))
        return {75, "锁检查的项目路径不匹配，保留原锁。"};
    if (!cache.is_absolute() || cache.filename().u8string() != cacheKey(project))
        return {75, "锁检查的缓存路径或项目标识不匹配，保留原锁。"};
    const bool owned = samePath(msys, cache / "msys64");
    if (!owned && !selectedEnvironment)
        return {75, "外部 MSYS2 未被当前入口选中，保留原锁。"};
    if (!msys.is_absolute() || !samePath(lock, msys / "var/lib/pacman/db.lck"))
        return {75, "数据库锁不是所选 MSYS2 的默认数据库锁，保留原锁。"};

    // Prevent directory replacement during inspection. Reject junctions and
    // other reparse points instead of following them outside the selected roots.
    std::vector<Handle> directories;
    for (const auto &directory : {cache, msys, msys / "var", msys / "var/lib", msys / "var/lib/pacman"}) {
        directories.emplace_back(
            CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!directories.back().valid() || !ordinary(directories.back().value, true))
            return {75, "无法确认数据库目录，或目录包含链接，保留原锁。"};
    }
    Handle marker(CreateFileW((cache / ".relaxer-owned-cache").c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!marker.valid() || !ordinary(marker.value, false))
        return {75, "无法确认 Relaxer 缓存归属，保留原锁。"};
    const auto owner = cacheOwner(project);
    std::string contents(owner.size() + 1, '\0');
    DWORD count = 0;
    if (!ReadFile(marker.value, contents.data(), DWORD(contents.size()), &count, nullptr) ||
        count != owner.size() || contents.compare(0, count, owner) != 0)
        return {75, "缓存归属标记不匹配，保留原锁。"};

    if (!owned) {
        // Existing installs need no Relaxer owner marker in their own directory.
        // The project cache must still be owned, and the selected root must
        // contain the actual MSYS2 programs, not just an arbitrary lock file.
        for (auto name : {"usr/bin/bash.exe", "usr/bin/pacman.exe", "usr/bin/msys-2.0.dll"}) {
            Handle program(CreateFileW((msys / name).c_str(), FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (!program.valid() || !ordinary(program.value, false, false))
                return {75, "所选目录不是完整的普通 MSYS2 安装，保留原锁。"};
        }
    }

    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!snapshot.valid() || !Process32FirstW(snapshot.value, &entry)) {
        const auto error = GetLastError();
        return {75, "无法检查更新进程，保留原锁。", false, error};
    }
    do {
        if (_wcsicmp(entry.szExeFile, L"pacman.exe") == 0)
            return {75, "检测到 pacman.exe 正在运行；等待更新完成，原锁保留。", true};
    } while (Process32NextW(snapshot.value, &entry));
    const auto enumerationError = GetLastError();
    if (enumerationError != ERROR_NO_MORE_FILES)
        return {75, "更新进程检查未完成，保留原锁。", false, enumerationError};

    auto opened = CreateFileW(lock.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | DELETE,
        0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    auto openError = opened == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    if (opened == INVALID_HANDLE_VALUE && openError == ERROR_ACCESS_DENIED) {
        // A non-read-only lock may be deletable even when attribute writes are
        // denied. The fallback cannot clear read-only without the needed right.
        opened = CreateFileW(lock.c_str(), FILE_READ_ATTRIBUTES | DELETE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        openError = opened == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    }
    Handle sentinel(opened);
    if (!sentinel.valid()) {
        if (openError == ERROR_FILE_NOT_FOUND)
            return {0, "数据库锁已消失，继续准备。"};
        return {75, "数据库锁仍被占用或无法打开，保留原锁。",
                openError == ERROR_SHARING_VIOLATION || openError == ERROR_LOCK_VIOLATION, openError};
    }
    if (!ordinary(sentinel.value, false))
        return {75, "数据库锁不是普通文件，保留原锁。"};

    FILE_BASIC_INFO original{};
    if (!GetFileInformationByHandleEx(sentinel.value, FileBasicInfo, &original, sizeof(original))) {
        const auto error = GetLastError();
        return {75, "无法读取锁文件属性，原锁保留。", false, error};
    }
    // Attribute changes, deletion and rollback all use the same exclusive
    // file handle. Only READONLY is removed; original times/other flags are kept.
    WindowsLockOps ops{sentinel.value, original};
    const auto removed = removeCheckedLock(original.FileAttributes, ops);
    Result result{removed.success ? 0 : 75,
        owned ? "已恢复 Relaxer 自有环境的遗留锁，继续准备。"
              : "已恢复所选已有 MSYS2 的遗留锁，继续复用。"};
    if (!removed.success) {
        switch (removed.failure) {
        case RemovalFailure::ClearReadOnly:
            result.message = "无法清除锁文件的只读属性，原锁保留。"; break;
        case RemovalFailure::VerifyAttributes:
            result.message = "清除只读属性后的核验失败，停止删除并尝试恢复原属性。"; break;
        case RemovalFailure::Delete:
            result.message = "无法删除所选 MSYS2 的遗留锁；已尝试恢复原属性，查看下方系统错误。"; break;
        default: break;
        }
    }
    result.systemError = removed.error;
    result.restoreError = removed.restoreError;
    result.originalAttributes = removed.originalAttributes;
    result.readOnlyCleared = removed.readOnlyCleared;
    result.readOnlyRestored = removed.readOnlyRestored;
    return result;
}
} // namespace pacman_lock
} // namespace relaxer::launch
