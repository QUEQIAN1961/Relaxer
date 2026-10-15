#include "readonly-lock-recovery.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace relaxer::launch::pacman_lock;
struct Ops {
    std::uint32_t attributes, original, error = 0;
    bool blockClear = false, blockRead = false, wrongRead = false;
    bool blockDelete = false, blockRestore = false, zeroDeleteError = false;
    unsigned deletes = 0;
    std::vector<std::uint32_t> changes;
    explicit Ops(std::uint32_t value) : attributes(value), original(value) {}
    bool setAttributes(std::uint32_t value) {
        changes.push_back(value);
        if ((blockClear && changes.size() == 1) || (blockRestore && changes.size() > 1)) {
            error = 5; return false;
        }
        attributes = value; error = 0; return true;
    }
    bool readAttributes(std::uint32_t &value) {
        if (blockRead) { error = 87; return false; }
        value = wrongRead ? original : attributes; error = 0; return true;
    }
    bool erase() {
        ++deletes;
        if (blockDelete) { error = zeroDeleteError ? 0 : 32; return false; }
        if (attributes & readonlyAttribute) { error = 5; return false; }
        error = 0; return true;
    }
    std::uint32_t lastError() const { return error; }
};
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
    unsigned passed = 0;
    auto pass = [&](const char *text) { ++passed; std::cout << "PASS: " << text << '\n'; };
    try {
        {
            Ops ops(0x20);
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(result.success && ops.changes.empty() && ops.deletes == 1, "normal lock changed attributes");
            pass("normal lock deletion needs no attribute mutation");
        }
        for (auto attributes : {0x21u, 0x27u}) {
            Ops ops(attributes);
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(result.success && result.readOnlyCleared && result.originalAttributes == attributes,
                    "read-only lock did not recover");
            require(ops.changes == std::vector<std::uint32_t>{attributes & ~readonlyAttribute},
                    "unrelated attribute flags changed");
            pass(attributes == 0x21 ? "reported 0x21 lock: clear READONLY, preserve ARCHIVE, then delete"
                                   : "hidden/system/archive flags survive READONLY clearing");
        }
        {
            Ops ops(1);
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(result.success && ops.changes == std::vector<std::uint32_t>{normalAttribute},
                    "zero attributes would leave read-only unchanged on Windows");
            pass("READONLY-only file uses NORMAL instead of zero/no-change");
        }
        {
            Ops ops(0x21); ops.blockClear = true;
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(!result.success && result.failure == RemovalFailure::ClearReadOnly && result.error == 5 &&
                    ops.deletes == 0 && ops.attributes == 0x21 && !result.readOnlyCleared,
                    "failed attribute clearing proceeded to deletion");
            pass("attribute access denied preserves lock and captures original error 5");
        }
        {
            Ops ops(0x21); ops.blockRead = true;
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(!result.success && result.error == 87 && result.readOnlyRestored && ops.deletes == 0 &&
                    ops.attributes == 0x21, "read failure was not rolled back");
            pass("attribute verification API failure restores original flags before stopping");
        }
        {
            Ops ops(0x21); ops.wrongRead = true;
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(!result.success && result.failure == RemovalFailure::VerifyAttributes && result.error == 0 &&
                    result.readOnlyRestored && ops.deletes == 0 && ops.attributes == 0x21,
                    "unchanged read-only attribute was ignored");
            pass("attribute mismatch stops deletion without inventing a Windows error");
        }
        {
            Ops ops(0x21); ops.blockDelete = true;
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(!result.success && result.error == 32 && result.readOnlyRestored && ops.attributes == 0x21,
                    "delete failure lost its error or original attributes");
            pass("delete error 32 survives successful attribute rollback");
        }
        {
            Ops ops(0x21); ops.blockDelete = ops.blockRestore = true;
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(!result.success && result.error == 32 && result.restoreError == 5 &&
                    !result.readOnlyRestored && result.readOnlyCleared,
                    "rollback failure hid the original delete error");
            pass("failed rollback records delete error 32 and restoration error 5 separately");
        }
        {
            Ops ops(0x21); ops.blockDelete = ops.zeroDeleteError = true;
            const auto result = removeCheckedLock(ops.attributes, ops);
            require(!result.success && result.error == 0 && result.readOnlyRestored,
                    "an API failure with no error code was treated as success");
            pass("false API result remains failure even if Windows error value is zero");
        }
        std::cout << passed << " readonly-lock logic scenarios passed (API fixtures, not Windows execution).\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
