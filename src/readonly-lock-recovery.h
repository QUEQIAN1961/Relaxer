#pragma once
#include <cstdint>

namespace relaxer::launch::pacman_lock {
// This algorithm runs only after the native caller validates ownership/path,
// processes, file kind and an exclusive handle. Ops acts on that same handle.
enum class RemovalFailure { None, ClearReadOnly, VerifyAttributes, Delete };
struct RemovalResult {
    bool success = false;
    RemovalFailure failure = RemovalFailure::None;
    std::uint32_t error = 0, restoreError = 0, originalAttributes = 0;
    bool readOnlyCleared = false, readOnlyRestored = false;
};
inline constexpr std::uint32_t readonlyAttribute = 0x1, normalAttribute = 0x80;
template <typename Ops>
RemovalResult removeCheckedLock(std::uint32_t attributes, Ops &ops) {
    RemovalResult result;
    result.originalAttributes = attributes;
    auto restore = [&] {
        if (!result.readOnlyCleared) return;
        result.readOnlyRestored = ops.setAttributes(attributes);
        if (!result.readOnlyRestored) result.restoreError = ops.lastError();
    };
    if (attributes & readonlyAttribute) {
        auto cleared = attributes & ~readonlyAttribute;
        // FILE_BASIC_INFO with attributes=0 leaves attributes unchanged.
        // NORMAL represents a file with no remaining attribute flags.
        if (!cleared) cleared = normalAttribute;
        if (!ops.setAttributes(cleared)) {
            result.failure = RemovalFailure::ClearReadOnly;
            result.error = ops.lastError();
            return result;
        }
        result.readOnlyCleared = true;
        std::uint32_t current = 0;
        if (!ops.readAttributes(current)) {
            result.failure = RemovalFailure::VerifyAttributes;
            result.error = ops.lastError();
            restore();
            return result;
        }
        if ((current & ~normalAttribute) != (cleared & ~normalAttribute)) {
            result.failure = RemovalFailure::VerifyAttributes;
            // The read succeeded, but the attributes did not match. Do not
            // invent a Windows API error or attempt deletion in this case.
            restore();
            return result;
        }
    }
    if (!ops.erase()) {
        result.failure = RemovalFailure::Delete;
        result.error = ops.lastError(); // capture before rollback overwrites it
        restore();
        return result;
    }
    result.success = true;
    return result;
}
} // namespace relaxer::launch::pacman_lock
