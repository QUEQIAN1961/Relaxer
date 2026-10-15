#pragma once
#include <array>
#include <string>
#include <vector>

namespace relaxer::launch {
enum class PrepareStage {
    Check,
    UpdateCore,
    UpdateRemaining,
    Install,
    Build,
    Download,
    Deploy,
    Verify,
    Cleanup
};
inline constexpr std::array<PrepareStage, 9> stageOrder{
    PrepareStage::Download,        PrepareStage::Check,   PrepareStage::UpdateCore,
    PrepareStage::UpdateRemaining, PrepareStage::Install, PrepareStage::Build,
    PrepareStage::Deploy,          PrepareStage::Verify,  PrepareStage::Cleanup};
inline const char *stageKey(PrepareStage stage) {
    switch (stage) {
    case PrepareStage::Check:
        return "check-dependencies";
    case PrepareStage::UpdateCore:
        return "update-core";
    case PrepareStage::UpdateRemaining:
        return "update-remaining";
    case PrepareStage::Install:
        return "install-dependencies";
    case PrepareStage::Build:
        return "build-relaxer";
    case PrepareStage::Download:
        return "prepare-online-environment";
    case PrepareStage::Deploy:
        return "collect-runtime";
    case PrepareStage::Verify:
        return "verify-portable-runtime";
    case PrepareStage::Cleanup:
        return "clean-owned-cache";
    }
    return "unknown";
}
inline const wchar_t *stageLabel(PrepareStage stage) {
    switch (stage) {
    case PrepareStage::Check:
        return L"检查依赖";
    case PrepareStage::UpdateCore:
        return L"更新 MSYS2";
    case PrepareStage::UpdateRemaining:
        return L"继续更新 MSYS2";
    case PrepareStage::Install:
        return L"安装并验证依赖";
    case PrepareStage::Build:
        return L"编译 Relaxer";
    case PrepareStage::Download:
        return L"下载并准备独立环境";
    case PrepareStage::Deploy:
        return L"收集运行库和插件";
    case PrepareStage::Verify:
        return L"验证独立运行";
    case PrepareStage::Cleanup:
        return L"清理临时编译缓存";
    }
    return L"未知阶段";
}
inline std::vector<std::wstring> bashArguments(PrepareStage stage) {
    const wchar_t *script = L"scripts/install-windows-dependencies.sh";
    const wchar_t *mode = L"--check";
    if (stage == PrepareStage::UpdateCore || stage == PrepareStage::UpdateRemaining)
        mode = L"--update";
    else if (stage == PrepareStage::Install)
        mode = L"--install";
    else if (stage == PrepareStage::Build) {
        script = L"scripts/build-windows-ucrt64.sh";
        mode = L"--launch";
    }
    return {L"--login", script, mode};
}
// Only the read-only check may return 1 to request installation. Every other
// process failure stops preparation immediately with its original result.
// Each invocation creates a fresh Bash process so MSYS2 core updates do not
// reuse a shell which has loaded the old runtime.
template <typename Run> auto runPreparation(Run run, bool interruptedUpdate = false) {
    auto failed = [](const auto &result) {
        return !result.started || result.systemError || result.readError;
    };
    auto result = run(PrepareStage::Check);
    if (failed(result) || result.code > 1)
        return result;
    if (result.code == 1 || interruptedUpdate) {
        for (auto stage : {PrepareStage::UpdateCore, PrepareStage::UpdateRemaining, PrepareStage::Install}) {
            result = run(stage);
            if (failed(result) || result.code)
                return result;
        }
    }
    return run(PrepareStage::Build);
}
// Commit and cleanup are reached only after a successful standalone check.
// A failed download, compile, deployment or verification keeps the owned cache.
template <typename Run> auto runFirstStart(bool downloadNeeded, Run run, bool interruptedUpdate = false) {
    auto failed = [](const auto &result) {
        return !result.started || result.systemError || result.readError || result.code;
    };
    if (downloadNeeded) {
        auto result = run(PrepareStage::Download);
        if (failed(result))
            return result;
    }
    auto result = runPreparation(run, interruptedUpdate);
    if (failed(result))
        return result;
    for (auto stage : {PrepareStage::Deploy, PrepareStage::Verify, PrepareStage::Cleanup}) {
        result = run(stage);
        if (failed(result))
            return result;
    }
    return result;
}
} // namespace relaxer::launch
