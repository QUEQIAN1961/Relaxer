// Exercise the actual bootstrap selector without opening preparation windows.
#define wWinMain relaxerBootstrapEntryForTest
#include "../src/start-windows.cpp"
#undef wWinMain
#include <iostream>

int wmain(int argc, wchar_t **argv) {
    if (argc > 1 && std::wstring(argv[1]) == L"--launcher-child") {
        Sleep(static_cast<DWORD>(std::stoul(argv[2])));
        return std::stoi(argv[3]);
    }
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
        using relaxer::launch::arguments;
        auto check = [](bool condition, const char *message) {
            if (!condition) throw std::runtime_error(message);
        };
        check(arguments({}).runtime == std::vector<std::wstring>{L"--launch", L"--ui=qml"}, "default is not QML");
        check(arguments({L"--widgets"}).runtime.back() == L"--ui=widgets", "Widgets alias failed");
        check(arguments({L"--qml"}).runtime == arguments({}).runtime, "explicit QML differs from default");
        check(arguments({L"--ui", L"widgets"}).runtime == arguments({L"--widgets"}).runtime, "runtime UI syntax differs");
        check(!arguments({L"--qml", L"--widgets"}).valid && !arguments({L"--ui=bad"}).valid &&
              !arguments({L"--ui"}).valid, "conflicting or invalid UI selectors accepted");
        check(arguments({L"--data", L"--widgets"}).runtime.back() == L"--widgets", "path value interpreted as a selector");
        const std::vector<std::wstring> forwarded{L"--data", L"中文 空格 path\\", L"--diagnostics", L"a \"quote\".json", L"", L"--page=settings"};
        const auto parsed = arguments(forwarded);
        check(parsed.diagnostics && parsed.valid, "diagnostic argument lost");
        const auto command = relaxer::launch::commandLine(L"C:/中文 路径/Relaxer.exe", parsed.runtime);
        int count = 0;
        auto decoded = CommandLineToArgvW(command.c_str(), &count);
        check(decoded && count == static_cast<int>(parsed.runtime.size() + 1), "quoted argv count changed");
        bool same = true;
        for (int i = 1; i < count; ++i) same &= decoded[i] == parsed.runtime[i - 1];
        LocalFree(decoded);
        check(same, "Windows quoting changed Unicode, spaces, empty values, quotes or trailing slashes");
        std::cout << "PASS: default/explicit UI, conflict rejection, forwarding and Windows argv roundtrip\n";
        wchar_t self[32768]{}; GetModuleFileNameW(nullptr, self, 32768);
        relaxer::launch::Arguments childArgs;
        childArgs.diagnostics = true;
        for (const auto &row : std::vector<std::pair<int, int>>{{0, 0}, {0, 11}, {2800, 3}}) {
            childArgs.runtime = {L"--launcher-child", std::to_wstring(row.first), std::to_wstring(row.second)};
            check(run(base, self, childArgs) == row.second, "launcher changed child exit code or detached early");
        }
        std::cout << "PASS: actual process exit 0/11 and delayed exit 3 propagated without detaching\n";
        set(L"RELAXER_MSYS2_DIR", saved);
        fs::remove_all(base);
        return 0;
    } catch (const std::exception &error) {
        set(L"RELAXER_MSYS2_DIR", saved);
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
