#pragma once
#include "launch-plan.h"
#include <vector>

namespace relaxer::launch {
struct Arguments {
    std::vector<std::wstring> runtime;
    bool diagnostics = false;
    bool valid = true;
};
// UI aliases belong to the launcher; the runtime retains its existing CLI.
// Consume option values before looking for aliases so paths are never selectors.
inline Arguments arguments(const std::vector<std::wstring> &input) {
    Arguments result;
    std::wstring selected;
    auto selectUi = [&](const std::wstring &value) {
        if ((value != L"qml" && value != L"widgets") ||
            (!selected.empty() && selected != value)) result.valid = false;
        selected = value;
    };
    for (size_t i = 0; i < input.size(); ++i) {
        const auto &arg = input[i];
        if (arg == L"--") {
            result.runtime.insert(result.runtime.end(), input.begin() + i, input.end());
            break;
        }
        if (arg == L"--qml" || arg == L"--widgets") selectUi(arg.substr(2));
        else if (arg.rfind(L"--ui=", 0) == 0) selectUi(arg.substr(5));
        else if (arg == L"--ui") {
            if (++i == input.size()) { result.valid = false; break; }
            selectUi(input[i]);
        } else {
            result.runtime.push_back(arg);
            if (arg == L"--diagnostics" || arg.rfind(L"--diagnostics=", 0) == 0)
                result.diagnostics = true;
            if (arg == L"--data" || arg == L"--diagnostics" || arg == L"--screenshot" ||
                arg == L"--page" || arg == L"--quit-after" || arg == L"--media-smoke" ||
                arg == L"--media-smoke-worker" || arg == L"--audio-device-smoke") {
                if (i + 1 < input.size()) result.runtime.push_back(input[++i]);
            }
        }
    }
    result.runtime.insert(result.runtime.begin(), {L"--launch", L"--ui=" +
        (selected.empty() ? std::wstring(L"qml") : selected)});
    return result;
}
inline std::wstring commandLine(const fs::path &exe, const std::vector<std::wstring> &args) {
    auto command = quote(exe.wstring());
    for (const auto &arg : args) command += L" " + quote(arg);
    return command;
}
}
