#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "launch-plan.h"
#include "windows-pacman-lock.h"
#include <iostream>

namespace fs = std::filesystem;
namespace lock = relaxer::launch::pacman_lock;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void write(const fs::path &path, const std::string &text) {
    std::ofstream(path, std::ios::binary) << text;
}
std::string read(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}
struct Fixture {
    fs::path root, cache, msys, sentinel;
    Fixture(const fs::path &base, const char *name) {
        root = base / name;
        fs::create_directories(root);
        cache = base / "cache" / relaxer::launch::cacheKey(root);
        relaxer::launch::claimCache(cache, root);
        msys = cache / "msys64";
        sentinel = msys / "var/lib/pacman/db.lck";
        fs::create_directories(sentinel.parent_path());
        write(sentinel, "");
        write(sentinel.parent_path() / "packages.keep", "installed database bytes");
        fs::create_directories(msys / "var/cache/pacman/pkg");
        write(msys / "var/cache/pacman/pkg/download.keep", "cached download bytes");
        fs::create_directories(root / "data");
        write(root / "data/keep", "user music and statistics");
    }
    lock::Result recover() { return lock::recover(root, root, cache, msys, sentinel); }
    void preserved() {
        require(fs::exists(sentinel), "lock must be preserved");
        require(read(sentinel.parent_path() / "packages.keep") == "installed database bytes",
                "package database changed");
        require(read(msys / "var/cache/pacman/pkg/download.keep") == "cached download bytes",
                "downloads changed");
        require(read(root / "data/keep") == "user music and statistics", "user data changed");
    }
};
int wmain(int argc, wchar_t **argv) {
    if (argc == 4 && std::wstring(argv[1]) == L"--pacman-fixture") {
        lock::Handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[2]));
        lock::Handle stop(OpenEventW(SYNCHRONIZE, FALSE, argv[3]));
        if (!ready.valid() || !stop.valid())
            return 3;
        SetEvent(ready.value);
        return WaitForSingleObject(stop.value, 30000) == WAIT_OBJECT_0 ? 0 : 4;
    }
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    auto base = fs::path(temp) / (L"Relaxer 锁测试 &!%() " + std::to_wstring(GetCurrentProcessId()));
    unsigned passed = 0;
    auto pass = [&](const char *label) {
        ++passed;
        std::cout << "PASS: " << label << std::endl;
    };
    try {
        fs::create_directories(base);
        {
            Fixture f(base, "orphan");
            require(f.recover().code == 0 && !fs::exists(f.sentinel), "orphan was not recovered");
            require(read(f.sentinel.parent_path() / "packages.keep") == "installed database bytes",
                    "database changed during recovery");
            require(read(f.msys / "var/cache/pacman/pkg/download.keep") == "cached download bytes",
                    "download cache changed during recovery");
            require(read(f.root / "data/keep") == "user music and statistics", "data changed");
            pass("orphan recovered; database, downloaded packages and user data unchanged");
            require(f.recover().code == 0, "missing lock should allow continuation");
            pass("already vanished lock is harmless");
        }
        {
            Fixture f(base, "project");
            require(lock::recover(f.root, base / "different", f.cache, f.msys, f.sentinel).code == 75,
                    "wrong source accepted");
            f.preserved();
            pass("different source project is protected");
        }
        {
            Fixture f(base, "marker");
            write(f.cache / ".relaxer-owned-cache", "owned by somebody else\n");
            require(f.recover().code == 75, "wrong owner accepted");
            f.preserved();
            pass("different owner marker is protected");
            fs::remove(f.cache / ".relaxer-owned-cache");
            require(f.recover().code == 75, "missing owner accepted");
            f.preserved();
            pass("missing owner marker is protected");
        }
        {
            Fixture f(base, "external");
            require(lock::recover(f.root, f.root, f.cache, base / "user-msys", f.sentinel).code == 75,
                    "external MSYS accepted");
            f.preserved();
            pass("user-installed MSYS2 is protected");
            auto custom = base / "custom-db.lck";
            write(custom, "external transaction");
            require(lock::recover(f.root, f.root, f.cache, f.msys, custom).code == 75,
                    "custom database accepted");
            require(read(custom) == "external transaction", "custom lock changed");
            f.preserved();
            pass("custom database path is protected");
        }
        {
            Fixture f(base, "selected-external");
            const auto external = base / "existing-msys";
            const auto externalLock = external / "var/lib/pacman/db.lck";
            fs::create_directories(externalLock.parent_path());
            fs::create_directories(external / "usr/bin");
            for (auto name : {"bash.exe", "pacman.exe", "msys-2.0.dll"})
                write(external / "usr/bin" / name, "ordinary program fixture");
            write(externalLock, "orphan transaction");
            require(SetFileAttributesW(externalLock.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
                    "external read-only fixture failed");
            write(externalLock.parent_path() / "installed.keep", "existing installed package database");
            write(external / "usr/bin/harbor.keep", "Harbor environment file");
            require(lock::recover(f.root, f.root, f.cache, external, externalLock).code == 75,
                    "unselected external root was accepted");
            require(fs::exists(externalLock), "unselected external lock changed");
            require(lock::recover(f.root, f.root, f.cache, external, externalLock, true).code == 0,
                    "selected existing environment could not recover its orphan");
            require(!fs::exists(externalLock), "selected orphan still exists");
            require(read(externalLock.parent_path() / "installed.keep") == "existing installed package database",
                    "existing package database changed");
            require(read(external / "usr/bin/harbor.keep") == "Harbor environment file",
                    "existing Harbor environment changed");
            pass("only the selected existing environment recovers its orphan; installed files unchanged");
            write(externalLock, "keep custom or incomplete root");
            auto custom = external / "custom-db.lck";
            write(custom, "custom database lock");
            require(lock::recover(f.root, f.root, f.cache, external, custom, true).code == 75,
                    "custom external database was accepted");
            require(read(custom) == "custom database lock", "custom external lock changed");
            pass("selected environment still protects a custom database");
            fs::remove(external / "usr/bin/pacman.exe");
            require(lock::recover(f.root, f.root, f.cache, external, externalLock, true).code == 75,
                    "incomplete external environment accepted");
            require(read(externalLock) == "keep custom or incomplete root", "incomplete-root lock changed");
            pass("incomplete selected install is protected");
        }
        {
            Fixture f(base, "held");
            require(SetFileAttributesW(f.sentinel.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
                    "held read-only fixture failed");
            lock::Handle held(CreateFileW(f.sentinel.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                          OPEN_EXISTING, 0, nullptr));
            require(held.valid(), "cannot create held-lock fixture");
            require(f.recover().code == 75, "open lock was deleted");
            f.preserved();
            require(GetFileAttributesW(f.sentinel.c_str()) & FILE_ATTRIBUTE_READONLY, "held file lost read-only");
            pass("held read-only file preserves both lock and attributes");
        }
        {
            Fixture f(base, "directory");
            fs::remove(f.sentinel);
            fs::create_directory(f.sentinel);
            require(f.recover().code == 75, "directory was deleted");
            f.preserved();
            pass("non-file lock is protected");
        }
        {
            Fixture f(base, "hard-link");
            auto other = base / "linked-lock.keep";
            require(CreateHardLinkW(other.c_str(), f.sentinel.c_str(), nullptr), "hard-link fixture failed");
            require(f.recover().code == 75, "hard-linked lock was deleted");
            f.preserved();
            require(fs::exists(other), "other link removed");
            pass("hard-linked lock is protected");
        }
        {
            Fixture f(base, "read-only");
            require(SetFileAttributesW(f.sentinel.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
                    "read-only fixture failed");
            const auto result = f.recover();
            require(result.code == 0 && result.readOnlyCleared && result.originalAttributes == 0x21 &&
                    !fs::exists(f.sentinel), "reported read-only/archive lock did not recover");
            require(read(f.sentinel.parent_path() / "packages.keep") == "installed database bytes", "database changed");
            require(read(f.root / "data/keep") == "user music and statistics", "user data changed");
            pass("real Windows read-only/archive lock recovered; package and user data unchanged");
        }
        {
            Fixture f(base, "readonly-attribute-right-denied");
            require(SetFileAttributesW(f.sentinel.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
                    "read-only fixture failed");
            lock::Handle held(CreateFileW(f.sentinel.c_str(), FILE_READ_ATTRIBUTES | DELETE, 0, nullptr,
                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            require(held.valid(), "cannot open attribute-right-denied fixture");
            FILE_BASIC_INFO before{};
            require(GetFileInformationByHandleEx(held.value, FileBasicInfo, &before, sizeof(before)), "read basic info");
            lock::WindowsLockOps ops{held.value, before};
            const auto result = lock::removeCheckedLock(before.FileAttributes, ops);
            require(!result.success && result.failure == lock::RemovalFailure::ClearReadOnly && result.error != 0,
                    "missing FILE_WRITE_ATTRIBUTES was ignored");
            require(GetFileAttributesW(f.sentinel.c_str()) & FILE_ATTRIBUTE_READONLY,
                    "failed clearing changed read-only attribute");
            pass("real Windows missing attribute-write access preserves read-only file and original error");
        }
        {
            Fixture f(base, "readonly-rollback");
            require(SetFileAttributesW(f.sentinel.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
                    "read-only fixture failed");
            lock::Handle held(CreateFileW(f.sentinel.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | DELETE,
                0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            require(held.valid(), "cannot open rollback fixture");
            FILE_BASIC_INFO before{};
            require(GetFileInformationByHandleEx(held.value, FileBasicInfo, &before, sizeof(before)), "read basic info");
            struct FailDelete : lock::WindowsLockOps {
                bool erase() { error = ERROR_SHARING_VIOLATION; return false; }
            } ops;
            ops.handle = held.value; ops.original = before;
            const auto result = lock::removeCheckedLock(before.FileAttributes, ops);
            FILE_BASIC_INFO after{};
            require(GetFileInformationByHandleEx(held.value, FileBasicInfo, &after, sizeof(after)), "read restored info");
            require(!result.success && result.error == ERROR_SHARING_VIOLATION && result.readOnlyRestored &&
                    before.FileAttributes == after.FileAttributes, "real attributes were not rolled back");
            require(before.CreationTime.QuadPart == after.CreationTime.QuadPart &&
                    before.LastWriteTime.QuadPart == after.LastWriteTime.QuadPart,
                    "rollback changed creation or data-write time");
            pass("injected delete failure rolls back real Windows attributes and retains original error");
        }
        {
            Fixture f(base, "active");
            require(SetFileAttributesW(f.sentinel.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
                    "active read-only fixture failed");
            wchar_t self[32768]{};
            require(GetModuleFileNameW(nullptr, self, 32768) != 0, "cannot locate test binary");
            auto pacman = base / "pacman.exe";
            fs::copy_file(self, pacman);
            auto id = std::to_wstring(GetCurrentProcessId());
            auto readyName = L"RelaxerLockReady-" + id, stopName = L"RelaxerLockStop-" + id;
            lock::Handle ready(CreateEventW(nullptr, TRUE, FALSE, readyName.c_str()));
            lock::Handle stop(CreateEventW(nullptr, TRUE, FALSE, stopName.c_str()));
            auto command = relaxer::launch::quote(pacman.wstring()) + L" --pacman-fixture " +
                           relaxer::launch::quote(readyName) + L" " + relaxer::launch::quote(stopName);
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            require(CreateProcessW(pacman.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                                   nullptr, base.c_str(), &startup, &process),
                    "cannot start pacman process fixture");
            lock::Handle child(process.hProcess), thread(process.hThread);
            require(WaitForSingleObject(ready.value, 10000) == WAIT_OBJECT_0, "fixture not ready");
            auto result = f.recover();
            bool stillRunning = WaitForSingleObject(child.value, 0) == WAIT_TIMEOUT;
            SetEvent(stop.value);
            require(WaitForSingleObject(child.value, 10000) == WAIT_OBJECT_0, "fixture did not finish");
            require(result.code == 75 && stillRunning, "active pacman not protected");
            f.preserved();
            require(GetFileAttributesW(f.sentinel.c_str()) & FILE_ATTRIBUTE_READONLY, "active lock lost read-only");
            pass("running pacman keeps read-only lock, attributes and process; no termination");
            require(f.recover().code == 0 && !fs::exists(f.sentinel), "retry did not recover orphan");
            pass("retry after pacman finishes recovers remaining orphan");
        }
        // Clean up only this test's remaining read-only fixture files after
        // all inspection handles/processes have left their scopes.
        for (const auto &entry : fs::recursive_directory_iterator(base)) {
            if (!entry.is_regular_file()) continue;
            const auto attributes = GetFileAttributesW(entry.path().c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) {
                auto cleared = attributes & ~FILE_ATTRIBUTE_READONLY;
                require(SetFileAttributesW(entry.path().c_str(), cleared ? cleared : FILE_ATTRIBUTE_NORMAL),
                        "test-fixture cleanup failed");
            }
        }
        fs::remove_all(base);
        std::cout << passed << " native Windows lock scenarios passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "\nFixture retained at " << base.u8string() << '\n';
        return 1;
    }
}
