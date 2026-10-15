#pragma once
#include "prepare-flow.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <string>

namespace relaxer::launch {
enum class StepState { Pending, Running, Complete, Skipped, Failed };
enum class BuildPhase { None, Configure, Compile, Test, Ffmpeg, FfmpegDone };
// Worker-owned state. The Win32 view receives an immutable copy through its
// message queue; no controls are accessed from the preparation thread.
struct PrepareProgress {
    static constexpr int rowCount = 7;
    std::array<StepState, 9> steps{};
    PrepareStage current = PrepareStage::Check;
    BuildPhase phase = BuildPhase::None;
    int percent = 0, jobs = 0, testsDone = 0, testsTotal = 0;
    std::wstring testName;
    bool determinate = false, finished = false, success = false, ffmpegSoft = false;
    std::wstring detail = L"正在准备首次启动环境…", error;
    std::string pendingLine;
    static int index(PrepareStage stage) { return static_cast<int>(stage); }
    static int rank(BuildPhase value) { return static_cast<int>(value); }
    static const wchar_t *rowLabel(int row) {
        switch (row) {
        case 0:
            return L"准备编译环境";
        case 1:
            return L"编译 Relaxer";
        case 2:
            return L"运行自动测试";
        case 3:
            return L"编译精简 FFmpeg";
        case 4:
            return L"收集运行库和插件";
        case 5:
            return L"验证独立运行";
        default:
            return L"清理临时编译缓存";
        }
    }
    void begin(PrepareStage stage) {
        current = stage;
        steps[index(stage)] = StepState::Running;
        pendingLine.clear();
        if (stage == PrepareStage::Build) {
            phase = BuildPhase::Configure;
            testsDone = testsTotal = 0;
            testName.clear();
            for (int i = 1; i <= 3; ++i)
                if (steps[i] == StepState::Pending)
                    steps[i] = StepState::Skipped;
        }
        const int milestones[] = {10, 16, 23, 30, 40, 0, 90, 95, 98};
        percent = std::max(percent, milestones[index(stage)]);
        determinate = false;
        switch (stage) {
        case PrepareStage::Check:
            detail = L"正在检查已有依赖；齐全时会跳过更新和安装。";
            break;
        case PrepareStage::UpdateCore:
            detail = L"正在更新 MSYS2 核心组件，请等待此步骤完成。";
            break;
        case PrepareStage::UpdateRemaining:
            detail = L"正在完成其余组件更新，完整输出已写入日志。";
            break;
        case PrepareStage::Install:
            detail = L"正在安装并验证所需依赖，下载时间取决于网络。";
            break;
        case PrepareStage::Build:
            detail = L"正在配置工程，随后编译 Relaxer 主程序。";
            break;
        case PrepareStage::Download:
            detail = L"正在下载并校验 MSYS2，随后初始化独立的 UCRT64 环境。";
            break;
        case PrepareStage::Deploy:
            detail = L"正在复制程序、Qt 插件、工具和它们需要的 DLL。";
            break;
        case PrepareStage::Verify:
            detail = L"正在排除 MSYS2 路径，验证程序和工具可以独立运行。";
            break;
        case PrepareStage::Cleanup:
            detail = L"验证已通过，正在清理 Relaxer 自己创建的临时环境和下载缓存。";
            break;
        }
    }
    void complete(PrepareStage stage) { steps[index(stage)] = StepState::Complete; }
    StepState rowState(int row) const {
        auto combine = [&](const PrepareStage *group, int count) {
            bool run = false, fail = false, pending = false, complete = false, skip = false;
            for (int i = 0; i < count; ++i) {
                switch (steps[index(group[i])]) {
                case StepState::Running:
                    run = true;
                    break;
                case StepState::Failed:
                    fail = true;
                    break;
                case StepState::Pending:
                    pending = true;
                    break;
                case StepState::Complete:
                    complete = true;
                    break;
                case StepState::Skipped:
                    skip = true;
                    break;
                }
            }
            if (fail)
                return StepState::Failed;
            if (run || (complete && pending))
                return StepState::Running;
            if (complete)
                return StepState::Complete;
            if (skip && !pending)
                return StepState::Skipped;
            return StepState::Pending;
        };
        if (row == 0) {
            const PrepareStage group[] = {PrepareStage::Download, PrepareStage::Check, PrepareStage::UpdateCore,
                                          PrepareStage::UpdateRemaining, PrepareStage::Install};
            return combine(group, 5);
        }
        const auto build = steps[index(PrepareStage::Build)];
        auto buildRow = [&](BuildPhase from, BuildPhase until) {
            if (build == StepState::Skipped)
                return StepState::Skipped;
            if (build == StepState::Pending)
                return StepState::Pending;
            const int here = rank(phase), start = rank(from), stop = rank(until);
            if (build == StepState::Complete || here >= stop)
                return here >= start || build == StepState::Complete ? StepState::Complete : StepState::Pending;
            if (build == StepState::Failed) {
                if (here >= start && here < stop)
                    return StepState::Failed;
                if (here >= stop)
                    return StepState::Complete;
                return StepState::Pending;
            }
            if (here >= start && here < stop)
                return StepState::Running;
            return StepState::Pending;
        };
        if (row == 1)
            return buildRow(BuildPhase::Configure, BuildPhase::Test);
        if (row == 2)
            return buildRow(BuildPhase::Test, BuildPhase::Ffmpeg);
        if (row == 3)
            return buildRow(BuildPhase::Ffmpeg, BuildPhase::FfmpegDone);
        if (row == 4)
            return steps[index(PrepareStage::Deploy)];
        if (row == 5)
            return steps[index(PrepareStage::Verify)];
        return steps[index(PrepareStage::Cleanup)];
    }
    int activeRow() const {
        if (finished)
            return -1;
        for (int i = 0; i < rowCount; ++i)
            if (rowState(i) == StepState::Running)
                return i;
        return -1;
    }
    std::wstring statusText(int row) const {
        if (row == 2 && rowState(row) == StepState::Running && testsTotal > 0)
            return std::to_wstring(testsDone) + L" / " + std::to_wstring(testsTotal);
        if (row == 3 && ffmpegSoft && rowState(3) != StepState::Running && rowState(3) != StepState::Failed)
            return L"未能编译";
        switch (rowState(row)) {
        case StepState::Running:
            return L"进行中";
        case StepState::Complete:
            return L"完成";
        case StepState::Skipped:
            return L"已满足，跳过";
        case StepState::Failed:
            return L"未完成";
        default:
            return L"等待";
        }
    }
    void line(const std::string &text) {
        if (finished)
            return;
        if (current == PrepareStage::Download) {
            if (text == "RELAXER_PHASE:extract") {
                detail = L"下载文件已通过校验，正在解压独立 MSYS2 环境。";
                determinate = false;
                return;
            }
            if (text == "RELAXER_PHASE:initialize") {
                detail = L"正在初始化独立 UCRT64 环境，随后检查和安装依赖。";
                determinate = false;
                return;
            }
            if (text == "RELAXER_PHASE:reuse-environment") {
                detail = L"已复用本项目的临时编译环境，无需重新下载。";
                determinate = false;
                return;
            }
            constexpr char prefix[] = "RELAXER_DOWNLOAD:";
            if (text.rfind(prefix, 0) != 0)
                return;
            long long bytes = 0, total = 0;
            auto end = text.data() + text.size();
            auto first = std::from_chars(text.data() + sizeof(prefix) - 1, end, bytes);
            if (first.ec != std::errc{} || first.ptr == end || *first.ptr != '/')
                return;
            auto second = std::from_chars(first.ptr + 1, end, total);
            if (second.ec != std::errc{} || second.ptr != end || bytes < 0 || total <= 0 || bytes > total ||
                total > 2147483647LL)
                return;
            percent = std::max(percent, int(9LL * bytes / total));
            determinate = true;
            detail = L"正在下载 MSYS2：" + std::to_wstring(bytes / 1048576) + L" / " +
                     std::to_wstring(total / 1048576) + L" MB。";
            return;
        }
        if (current != PrepareStage::Build)
            return;
        if (text.rfind("RELAXER_JOBS:", 0) == 0) {
            int parsed = 0;
            auto end = text.data() + text.size();
            auto parsedJobs = std::from_chars(text.data() + 13, end, parsed);
            if (parsedJobs.ec == std::errc{} && parsedJobs.ptr == end && parsed > 0 && parsed <= 256)
                jobs = parsed;
            if (phase == BuildPhase::Configure && jobs > 0)
                detail = L"正在配置工程，随后使用 " + std::to_wstring(jobs) + L" 个核心编译。";
            return;
        }
        if (text.find("RELAXER_PHASE:configure") != std::string::npos) {
            phase = BuildPhase::Configure;
            detail = jobs > 0 ? L"正在配置工程，随后使用 " + std::to_wstring(jobs) + L" 个核心编译。"
                              : L"正在配置工程和检查编译环境。";
            determinate = false;
            return;
        }
        if (text.find("RELAXER_PHASE:compile") != std::string::npos) {
            phase = BuildPhase::Compile;
            detail = jobs > 0 ? L"正在使用 " + std::to_wstring(jobs) + L" 个核心编译 Relaxer。"
                              : L"正在编译 Relaxer；读取到编译数量后显示实际进度。";
            determinate = false;
            return;
        }
        if (text.find("RELAXER_PHASE:test") != std::string::npos) {
            phase = BuildPhase::Test;
            detail = L"正在运行自动测试。";
            determinate = false;
            percent = std::max(percent, 62);
            return;
        }
        if (phase == BuildPhase::Test && text.rfind("RELAXER_TEST_TOTAL:", 0) == 0) {
            int total = 0;
            const auto end = text.data() + text.size();
            const auto parsed = std::from_chars(text.data() + 19, end, total);
            if (parsed.ec == std::errc{} && parsed.ptr == end && total > 0 && total <= 1000000) {
                testsTotal = total;
                determinate = true;
                detail = L"自动测试 0 / " + std::to_wstring(testsTotal) + L" 组。";
            }
            return;
        }
        if (text.find("RELAXER_PHASE:ffmpeg-local") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = L"正在使用包内的 FFmpeg 源码，无需下载。";
            determinate = true;
            percent = std::max(percent, 76);
            return;
        }
        if (text.find("RELAXER_PHASE:ffmpeg-download") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = L"正在下载精简 FFmpeg 源码。";
            determinate = false;
            percent = std::max(percent, 74);
            return;
        }
        if (text.find("RELAXER_PHASE:ffmpeg-configure") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = L"正在配置精简 FFmpeg。";
            determinate = true;
            percent = std::max(percent, 80);
            return;
        }
        if (text.find("RELAXER_PHASE:ffmpeg-compile") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = jobs > 0 ? L"正在使用 " + std::to_wstring(jobs) + L" 个核心编译精简 FFmpeg。"
                              : L"正在编译精简 FFmpeg。";
            determinate = true;
            percent = std::max(percent, 84);
            return;
        }
        if (text.find("RELAXER_PHASE:ffmpeg") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = L"正在准备精简 FFmpeg。";
            determinate = false;
            percent = std::max(percent, 74);
            return;
        }
        if (text.find("Slim FFmpeg already built.") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = L"精简 FFmpeg 已就绪，无需重新编译。";
            percent = std::max(percent, 88);
            return;
        }
        if (text.find("精简 FFmpeg 未能编译") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            ffmpegSoft = true;
            detail = L"精简 FFmpeg 未能编译。常见格式和 MIDI 仍可播放。";
            return;
        }
        if (text.find("Using bundled FFmpeg source") != std::string::npos) {
            phase = BuildPhase::Ffmpeg;
            detail = L"正在解压包内的 FFmpeg 源码。";
            percent = std::max(percent, 78);
            return;
        }
        if (phase == BuildPhase::Configure || phase == BuildPhase::Compile) {
            const auto bracket = text.find('[');
            if (bracket == std::string::npos)
                return;
            int done = -1, total = 0;
            const auto end = text.data() + text.size();
            const auto first = std::from_chars(text.data() + bracket + 1, end, done);
            if (first.ec != std::errc{} || first.ptr == end || *first.ptr != '/')
                return;
            const auto second = std::from_chars(first.ptr + 1, end, total);
            if (second.ec != std::errc{} || second.ptr == end || *second.ptr != ']' || done < 0 || total <= 0 ||
                done > total || total > 1000000)
                return;
            phase = BuildPhase::Compile;
            percent = std::max(percent, 40 + int(22LL * done / total));
            determinate = true;
            detail = (jobs > 0 ? L"正在使用 " + std::to_wstring(jobs) + L" 个核心编译："
                               : L"正在编译 Relaxer：") +
                     std::to_wstring(done) + L" / " + std::to_wstring(total) + L" 项。";
            return;
        }
        if (phase == BuildPhase::Test) {
            int done = 0, total = 0;
            if (leadingFraction(text, done, total, "Test")) {
                testsDone = std::max(testsDone, done);
                testsTotal = total;
                percent = std::max(percent, 62 + int(12LL * done / total));
                determinate = true;
                detail = L"自动测试 " + std::to_wstring(testsDone) + L" / " + std::to_wstring(testsTotal) + L" 组已完成。";
                return;
            }
            const auto started = text.find("Start ");
            const auto colon = started == std::string::npos ? std::string::npos : text.find(':', started);
            if (colon != std::string::npos) {
                auto name = text.substr(colon + 1);
                while (!name.empty() && name.front() == ' ')
                    name.erase(name.begin());
                const auto stop = name.find(' ');
                if (stop != std::string::npos)
                    name.resize(stop);
                if (!name.empty() && name.size() < 80) {
                    testName = widen(name);
                    detail = L"自动测试 " + std::to_wstring(testsDone) + L" / " +
                             (testsTotal > 0 ? std::to_wstring(testsTotal) : L"待统计") +
                             L" 组已完成，正在运行 " + testName + L"。";
                }
            }
            return;
        }
        if (phase == BuildPhase::Ffmpeg) {
            int curlPercent = 0;
            std::string received, totalSize;
            if (curlMeter(text, curlPercent, received, totalSize)) {
                percent = std::max(percent, 74 + int(6LL * curlPercent / 100));
                determinate = true;
                detail = L"正在下载精简 FFmpeg 源码：" + widen(received) + L" / " + widen(totalSize) + L"。";
                return;
            }
            if (text.rfind("CC", 0) == 0 || text.rfind("LD", 0) == 0 || text.rfind("AR", 0) == 0 ||
                text.find("libav") != std::string::npos) {
                auto file = text;
                const auto slash = file.find_last_of("/\\");
                if (slash != std::string::npos)
                    file.erase(0, slash + 1);
                if (file.size() > 48)
                    file.resize(48);
                if (!file.empty())
                    detail = L"正在编译精简 FFmpeg：" + widen(file);
            }
        }
    }
    void feed(const char *data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            if (data[i] == '\n' || data[i] == '\r') {
                if (!pendingLine.empty())
                    line(pendingLine);
                pendingLine.clear();
            } else if (pendingLine.size() < 8192)
                pendingLine += data[i];
        }
    }
    void fail(const std::wstring &message) {
        steps[index(current)] = StepState::Failed;
        finished = true;
        success = false;
        determinate = true;
        error = message;
        detail = L"准备未完成。可查看错误详情和日志，修复问题后点击“重试”。";
    }
    void done() {
        auto &build = steps[index(PrepareStage::Build)];
        if (build == StepState::Running || build == StepState::Pending)
            build = StepState::Complete;
        if (phase != BuildPhase::None && phase != BuildPhase::FfmpegDone)
            phase = BuildPhase::FfmpegDone;
        percent = 100;
        determinate = true;
        finished = success = true;
        detail = ffmpegSoft ? L"精简 FFmpeg 未能编译。常见格式和 MIDI 仍可播放。正在打开 Relaxer。"
                            : L"编译和独立运行检查已通过，临时缓存已清理。正在打开 Relaxer。";
    }

  private:
    static std::wstring widen(const std::string &text) {
        std::wstring out;
        out.reserve(text.size());
        for (unsigned char c : text)
            out.push_back(c < 128 ? wchar_t(c) : L'?');
        return out;
    }
    static bool leadingFraction(const std::string &text, int &done, int &total, const char *tail) {
        size_t i = 0;
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
            ++i;
        auto readInt = [&](int &out) {
            if (i >= text.size() || text[i] < '0' || text[i] > '9')
                return false;
            int value = 0;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                value = value * 10 + (text[i] - '0');
                if (value > 1000000)
                    return false;
                ++i;
            }
            out = value;
            return true;
        };
        if (!readInt(done) || i >= text.size() || text[i] != '/')
            return false;
        ++i;
        if (!readInt(total) || total <= 0 || done < 0 || done > total)
            return false;
        while (i < text.size() && text[i] == ' ')
            ++i;
        const auto length = std::char_traits<char>::length(tail);
        return text.compare(i, length, tail) == 0;
    }
    static bool curlMeter(const std::string &text, int &percent, std::string &received, std::string &totalSize) {
        auto integer = [&](size_t &i, int &out) {
            while (i < text.size() && text[i] == ' ')
                ++i;
            if (i >= text.size() || text[i] < '0' || text[i] > '9')
                return false;
            int value = 0;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                value = value * 10 + (text[i] - '0');
                if (value > 1000)
                    return false;
                ++i;
            }
            out = value;
            return true;
        };
        auto sizeToken = [&](size_t &i, std::string &out) {
            while (i < text.size() && text[i] == ' ')
                ++i;
            const auto start = i;
            bool digit = false, dot = false;
            while (i < text.size()) {
                const char c = text[i];
                if (c >= '0' && c <= '9') {
                    digit = true;
                    ++i;
                    continue;
                }
                if (c == '.' && !dot) {
                    dot = true;
                    ++i;
                    continue;
                }
                break;
            }
            if (!digit)
                return false;
            if (i < text.size()) {
                const char unit = text[i];
                if (unit == 'k' || unit == 'K' || unit == 'm' || unit == 'M' || unit == 'g' || unit == 'G')
                    ++i;
            }
            out = text.substr(start, i - start);
            return true;
        };
        for (size_t i = 0; i < text.size(); ++i) {
            size_t cursor = i;
            int first = 0, second = 0;
            std::string total, got;
            if (!integer(cursor, first) || first > 100 || !sizeToken(cursor, total))
                continue;
            const char unit = total.back();
            if (unit != 'M' && unit != 'm' && unit != 'K' && unit != 'k' && unit != 'G' && unit != 'g')
                continue;
            if (!integer(cursor, second) || second > 100 || !sizeToken(cursor, got))
                continue;
            percent = second;
            received = got;
            totalSize = total;
            return true;
        }
        return false;
    }
};
} // namespace relaxer::launch
