#pragma once
#include <filesystem>
#include <string>
namespace relaxer::launch {
namespace fs = std::filesystem;
enum class Kind { Portable, BuiltSource, PrepareSource, Incomplete };
struct Plan {
    Kind kind;
    fs::path root, executable;
};
inline Plan select(const fs::path &root) {
    if (fs::is_regular_file(root / "runtime/Relaxer.exe") &&
        fs::is_regular_file(root / "runtime/relaxer-ready.json"))
        return {Kind::Portable, root, root / "runtime/Relaxer.exe"};
    if (fs::is_regular_file(root / "Relaxer.exe") && fs::is_regular_file(root / "relaxer-ready.json"))
        return {Kind::Portable, root, root / "Relaxer.exe"};
    if (!fs::is_regular_file(root / "CMakeLists.txt") ||
        !fs::is_regular_file(root / "scripts/prepare-windows-launch.cmd") ||
        !fs::is_regular_file(root / "resources/icon.ico"))
        return {Kind::Incomplete, root, {}};
    auto exe = root / "build-windows/Relaxer.exe";
    return {fs::is_regular_file(exe) ? Kind::BuiltSource : Kind::PrepareSource, root, exe};
}
inline std::wstring quote(const std::wstring &arg) {
    std::wstring r = L"\"";
    size_t n = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++n;
            continue;
        }
        r.append(c == L'"' ? n * 2 + 1 : n, L'\\');
        r += c;
        n = 0;
    }
    r.append(n * 2, L'\\');
    return r + L'"';
}
} // namespace relaxer::launch
