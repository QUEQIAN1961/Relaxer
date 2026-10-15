// Exercise the actual bootstrap selector without opening preparation windows.
#define wWinMain relaxerBootstrapEntryForTest
#include "../src/start-windows.cpp"
#undef wWinMain
#include <iostream>

int wmain() {
    wchar_t temporary[32768]{};
    GetTempPathW(32768, temporary);
    const auto base = fs::path(temporary) / (L"RelaxerEnvTest-" + std::to_wstring(GetCurrentProcessId()));
    const auto saved = env(L"RELAXER_MSYS2_DIR");
    try {
        fs::create_directories(base / "shared/usr/bin");
        fs::create_directories(base / "shared/ucrt64");
        std::ofstream(base / "shared/usr/bin/bash.exe") << "selector fixture";
        std::ofstream(base / "msys2-path.txt") << (base / "shared").u8string() << '\n';
        set(L"RELAXER_MSYS2_DIR", L"");
        if (!relaxer::launch::pacman_lock::samePath(findMsys(base), base / "shared"))
            throw std::runtime_error("saved MSYS2 environment was not reused");
        std::cout << "PASS: saved MSYS2 environment is reused without downloading another installation\n";
        set(L"RELAXER_MSYS2_DIR", (base / "shared").wstring());
        if (!relaxer::launch::pacman_lock::samePath(findMsys(base), base / "shared"))
            throw std::runtime_error("explicit valid override was ignored");
        std::cout << "PASS: explicit valid override takes priority\n";
        set(L"RELAXER_MSYS2_DIR", (base / "missing").wstring());
        bool rejected = false;
        try { (void)findMsys(base); } catch (const std::exception &) { rejected = true; }
        if (!rejected) throw std::runtime_error("incomplete explicit override accepted");
        std::cout << "PASS: invalid override cannot silently pick a different environment\n";
        const auto cache = base / "build-cache";
        fs::create_directories(cache / "build");
        std::ofstream(cache / "build/CMakeCache.txt") << "CMAKE_CXX_COMPILER:FILEPATH="
            << (base / "shared/ucrt64/bin/g++.exe").u8string() << '\n';
        if (buildDirectory(cache, base / "shared") != cache / "build")
            throw std::runtime_error("matching compiler cache was not reused");
        if (buildDirectory(cache, base / "different") == cache / "build")
            throw std::runtime_error("compiler cache from a different MSYS2 was mixed in");
        std::cout << "PASS: matching build cache reused; changing environment cannot mix old objects/runtime\n";
        set(L"RELAXER_MSYS2_DIR", saved);
        fs::remove_all(base);
        return 0;
    } catch (const std::exception &error) {
        set(L"RELAXER_MSYS2_DIR", saved);
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
