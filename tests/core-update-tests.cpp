#include "core-update-restart.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace relaxer::launch;
struct Result {
    bool started = true;
    std::uint32_t code = 0, systemError = 0, readError = 0;
};
const std::string restartOutput =
    ":: Processing package changes...\nupgrading msys2-runtime...\n"
    ":: To complete this update all MSYS2 processes including this terminal will be closed.\n"
    "Confirm to proceed [Y/n]";
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
static CoreUpdateRestart output(const std::string &text, size_t chunk = 8192) {
    CoreUpdateRestart detector;
    for (size_t offset = 0; offset < text.size(); offset += chunk)
        detector.feed(text.data() + offset, std::min(chunk, text.size() - offset));
    return detector;
}
int main() {
    try {
        using S = PrepareStage;
        for (size_t chunk : {1u, 7u, 8192u})
            require(isCoreUpdateRestart(S::UpdateCore, Result{true, 1, 0, 0}, output(restartOutput, chunk)),
                    "expected restart was not recognized across pipe chunks");
        std::cout << "PASS: complete core-restart output, exit 1 and split pipe chunks\n";
        for (auto result : {Result{true, 37, 0, 0}, Result{true, 75, 0, 0}, Result{true, 143, 0, 0},
                            Result{false, 1, 0, 0}, Result{true, 1, 2, 0}, Result{true, 1, 0, 109}})
            require(!isCoreUpdateRestart(S::UpdateCore, result, output(restartOutput)),
                    "unrelated exit or launch/pipe failure was bypassed");
        std::cout << "PASS: unrelated codes, process creation and pipe errors stay failures\n";
        for (auto stage : {S::Check, S::Download, S::Install, S::Build, S::Verify, S::Cleanup})
            require(!isCoreUpdateRestart(stage, Result{true, 1, 0, 0}, output(restartOutput)),
                    "restart exception leaked into another stage");
        std::cout << "PASS: restart exception is limited to full-update stages\n";
        for (auto text : {std::string(), std::string("upgrading msys2-runtime...\n"),
                          std::string("Confirm to proceed [Y/n]"),
                          restartOutput.substr(restartOutput.find(":: To complete")),
                          restartOutput.substr(0, restartOutput.find("Confirm"))})
            require(!isCoreUpdateRestart(S::UpdateCore, Result{true, 1, 0, 0}, output(text)),
                    "incomplete evidence was accepted");
        std::cout << "PASS: exit 1 alone and incomplete shutdown/upgrade evidence are rejected\n";
        for (auto error :
             {"error: failed to retrieve file\n", "Errors occurred, no packages were upgraded.\n",
              "*** fatal error - cygheap base mismatch detected\n", "unable to commit transaction\n"}) {
            require(!isCoreUpdateRestart(S::UpdateCore, Result{true, 1, 0, 0},
                                         output(std::string(error) + restartOutput, 1)),
                    "error before restart prompt was ignored");
            require(
                !isCoreUpdateRestart(S::UpdateCore, Result{true, 1, 0, 0}, output(restartOutput + error, 7)),
                "error after restart prompt was ignored");
        }
        require(!isCoreUpdateRestart(
                    S::UpdateCore, Result{true, 1, 0, 0},
                    output("error: signature failure\n" + std::string(100000, '.') + restartOutput)),
                "an error lost from the tail was forgotten");
        std::cout
            << "PASS: transaction errors stay sticky before/after the prompt and beyond retained tail\n";
        auto execute = [&](bool fixed, bool earlierError, unsigned remaining, unsigned install,
                           std::vector<S> &calls) {
            return runFirstStart(false, [&](S stage) {
                calls.push_back(stage);
                Result result;
                if (stage == S::Check)
                    result.code = 1;
                if (stage == S::UpdateCore) {
                    result.code = 1;
                    auto detector = output((earlierError ? "error: checksum failure\n" : "") + restartOutput);
                    if (fixed && isCoreUpdateRestart(stage, result, detector))
                        result.code = 0;
                }
                if (stage == S::UpdateRemaining)
                    result.code = remaining;
                if (stage == S::Install)
                    result.code = install;
                return result;
            });
        };
        std::vector<S> calls;
        auto result = execute(false, false, 0, 0, calls);
        require(result.code == 1 && calls == std::vector<S>{S::Check, S::UpdateCore},
                "old controller did not reproduce the screenshot's failure");
        std::cout << "PASS: previous behavior reproduces stop at first core update / exit 1\n";
        calls.clear();
        result = execute(true, false, 0, 0, calls);
        require(result.code == 0 &&
                    calls == std::vector<S>{S::Check, S::UpdateCore, S::UpdateRemaining, S::Install, S::Build,
                                            S::Deploy, S::Verify, S::Cleanup},
                "recognized restart did not continue through fresh update and mandatory verification");
        std::cout << "PASS: recognized restart continues with a fresh update before install/build/verify\n";
        calls.clear();
        result = execute(true, true, 0, 0, calls);
        require(result.code == 1 && calls.back() == S::UpdateCore, "real core-update error was bypassed");
        calls.clear();
        result = execute(true, false, 1, 0, calls);
        require(result.code == 1 && calls.back() == S::UpdateRemaining, "fresh-update error was bypassed");
        calls.clear();
        result = execute(true, false, 0, 75, calls);
        require(result.code == 75 && calls.back() == S::Install, "pending upgrades allowed build or cleanup");
        std::cout
            << "PASS: genuine core/fresh-update and pending-upgrade failures still block later stages\n";
        for (auto stage : {S::UpdateCore, S::UpdateRemaining}) {
            unsigned runs = 0;
            auto restarted = runRestartableUpdate(stage, [&](CoreUpdateRestart &detector) {
                ++runs;
                if (runs < 3) {
                    detector.feed(restartOutput.data(), restartOutput.size());
                    return Result{true, 1, 0, 0};
                }
                return Result{};
            }, [](const std::string &) {});
            require(runs == 3 && restarted.code == 0, "fresh update was not required to succeed");
            runs = 0;
            restarted = runRestartableUpdate(stage, [&](CoreUpdateRestart &detector) {
                ++runs;
                detector.feed(restartOutput.data(), restartOutput.size());
                return Result{true, 1, 0, 0};
            }, [](const std::string &) {});
            require(runs == 3 && restarted.code == 75, "endless restarts were not bounded");
            runs = 0;
            restarted = runRestartableUpdate(stage, [&](CoreUpdateRestart &detector) {
                ++runs;
                if (runs == 1) detector.feed(restartOutput.data(), restartOutput.size());
                return Result{true, runs == 1 ? 1u : 37u, 0, 0};
            }, [](const std::string &) {});
            require(runs == 2 && restarted.code == 37, "error after restart was ignored");
        }
        std::cout << "PASS: both update phases restart, require a real success, cap loops and retain later errors\n";
        auto pending = false;
        unsigned checks = 0, updates = 0;
        auto resume = [&] {
            return runPreparation([&](S stage) {
                Result r;
                if (stage == S::Check) ++checks;
                if (stage == S::UpdateCore) { ++updates; r.code = 37; }
                return r;
            }, pending);
        };
        require(resume().code == 0 && updates == 0, "complete environment should build directly");
        pending = true;
        require(resume().code == 37 && checks == 2 && updates == 1, "interrupted update bypassed controller");
        std::cout << "PASS: interrupted-update marker resumes full updates even when all packages are installed\n";
        {
            std::vector<S> reused;
            auto ready = runFirstStart(false, [&](S stage) { reused.push_back(stage); return Result{}; });
            require(ready.code == 0 && reused == std::vector<S>{S::Check, S::Build, S::Deploy, S::Verify, S::Cleanup},
                    "existing ready environment downloaded or updated unnecessarily");
            std::cout << "PASS: existing ready environment skips download, both updates and installation\n";
            reused.clear();
            ready = runFirstStart(false, [&](S stage) {
                reused.push_back(stage);
                return Result{true, stage == S::Check ? 1u : 0u, 0, 0};
            });
            require(ready.code == 0 && std::find(reused.begin(), reused.end(), S::Download) == reused.end() &&
                    std::find(reused.begin(), reused.end(), S::Install) != reused.end(),
                    "existing incomplete dependency set downloaded a new environment");
            std::cout << "PASS: missing audio dependencies prepare inside the reused environment without downloading MSYS2\n";
        }
        std::cout << "All core-update restart regressions passed.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
