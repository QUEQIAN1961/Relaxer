#pragma once
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace relaxer::launch {
inline std::string cacheKey(const std::filesystem::path &project) {
    unsigned long long hash = 14695981039346656037ULL;
    for (unsigned char ch : project.lexically_normal().u8string()) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << "hb-" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}
inline std::string cacheOwner(const std::filesystem::path &project) {
    return "Relaxer-owned-cache-v1\n" + project.lexically_normal().u8string() + "\n";
}
inline bool ownedCache(const std::filesystem::path &cache, const std::filesystem::path &project) {
    namespace fs = std::filesystem;
    if (cache.filename().u8string() != cacheKey(project) || fs::is_symlink(cache) ||
        fs::is_symlink(cache / ".relaxer-owned-cache"))
        return false;
#ifdef _WIN32
    auto flags = GetFileAttributesW(cache.c_str());
    if (flags == INVALID_FILE_ATTRIBUTES || (flags & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;
#endif
    std::ifstream input(cache / ".relaxer-owned-cache", std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return input.good() && contents == cacheOwner(project);
}
inline void claimCache(const std::filesystem::path &cache, const std::filesystem::path &project) {
    namespace fs = std::filesystem;
    if (fs::exists(cache)) {
        if (!ownedCache(cache, project))
            throw std::runtime_error("Build cache is not owned by this Relaxer project");
        return;
    }
    fs::create_directories(cache);
    std::ofstream owner(cache / ".relaxer-owned-cache", std::ios::binary);
    owner << cacheOwner(project);
    owner.close();
    if (!ownedCache(cache, project))
        throw std::runtime_error("Cannot mark Relaxer's build cache");
}
#ifdef _WIN32
// Windows junctions and other reparse points must be removed as links; never
// descend into them, even if a third-party package created one in the cache.
inline void removeCacheTree(const std::filesystem::path &path) {
    auto flags = GetFileAttributesW(path.c_str());
    if (flags == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
            return;
        throw std::runtime_error("Cannot read build cache attributes");
    }
    if ((flags & FILE_ATTRIBUTE_DIRECTORY) && !(flags & FILE_ATTRIBUTE_REPARSE_POINT)) {
        WIN32_FIND_DATAW found{};
        auto handle = FindFirstFileW((path / L"*").c_str(), &found);
        if (handle != INVALID_HANDLE_VALUE) {
            try {
                do {
                    if (std::wstring(found.cFileName) != L"." && std::wstring(found.cFileName) != L"..")
                        removeCacheTree(path / found.cFileName);
                } while (FindNextFileW(handle, &found));
                auto error = GetLastError();
                FindClose(handle);
                handle = INVALID_HANDLE_VALUE;
                if (error != ERROR_NO_MORE_FILES)
                    throw std::runtime_error("Cannot enumerate build cache");
            } catch (...) {
                if (handle != INVALID_HANDLE_VALUE)
                    FindClose(handle);
                throw;
            }
        } else if (GetLastError() != ERROR_FILE_NOT_FOUND)
            throw std::runtime_error("Cannot enumerate build cache");
    }
    if (flags & FILE_ATTRIBUTE_READONLY)
        SetFileAttributesW(path.c_str(), flags & ~FILE_ATTRIBUTE_READONLY);
    if (!(flags & FILE_ATTRIBUTE_DIRECTORY ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str())))
        throw std::runtime_error("Build cache is still in use or cannot be deleted");
}
#endif
inline void cleanOwnedCache(const std::filesystem::path &cache, const std::filesystem::path &project) {
    if (!ownedCache(cache, project))
        throw std::runtime_error("Refusing to delete an unowned build cache");
    // Preserve the marker if cleanup fails, so a later launch can retry safely.
    for (const auto &entry : std::filesystem::directory_iterator(cache)) {
        if (entry.path().filename() == ".relaxer-owned-cache")
            continue;
#ifdef _WIN32
        removeCacheTree(entry.path());
#else
        std::filesystem::remove_all(entry.path());
#endif
    }
    std::filesystem::remove(cache / ".relaxer-owned-cache");
    try {
        if (!std::filesystem::remove(cache))
            throw std::runtime_error("Cannot remove the empty build cache");
    } catch (...) {
        if (std::filesystem::is_directory(cache) && !std::filesystem::is_symlink(cache)) {
            std::ofstream owner(cache / ".relaxer-owned-cache", std::ios::binary);
            owner << cacheOwner(project);
        }
        throw;
    }
}
} // namespace relaxer::launch
