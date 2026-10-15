#pragma once
#include "prepare-flow.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace relaxer::launch {
// Watch only the current update process. Keep evidence across pipe chunks,
// including errors printed before the tail retained by the error dialog.
class CoreUpdateRestart {
    std::string m_tail;
    bool m_changed = false, m_shutdown = false, m_confirm = false, m_error = false;

  public:
    void feed(const char *bytes, size_t length) {
        m_tail.append(bytes, length);
        std::string text = m_tail;
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        auto has = [&](const char *message) { return text.find(message) != std::string::npos; };
        for (auto name : {"msys2-runtime", "msys2-runtime-devel", "pacman", "bash", "filesystem", "mintty"})
            m_changed |= has((std::string("upgrading ") + name).c_str()) ||
                         has((std::string("installing ") + name).c_str());
        m_shutdown |=
            has("to complete this update all msys2 processes including this terminal will be closed.");
        m_confirm |= has("confirm to proceed [y/n]");
        for (auto failure :
             {"error:", "errors occurred", "fatal", "failed", "could not", "unable to", "cannot "})
            m_error |= has(failure);
        if (m_tail.size() > 4096)
            m_tail.erase(0, m_tail.size() - 4096);
    }
    bool expectedExit(unsigned long code) const {
        return code == 1 && m_changed && m_shutdown && m_confirm && !m_error;
    }
};
template <typename Result>
bool isCoreUpdateRestart(PrepareStage stage, const Result &result, const CoreUpdateRestart &output) {
    // Only full update phases may restart with complete shutdown evidence.
    // Install/build failures and Windows launch/pipe errors remain failures.
    return (stage == PrepareStage::UpdateCore || stage == PrepareStage::UpdateRemaining) && result.started && !result.systemError && !result.readError &&
           output.expectedExit(result.code);
}
// A restart is a transition, never success: only a later fresh process with
// exit 0 may finish the update. Bound retries so broken installations stop.
template <typename Run, typename Note>
auto runRestartableUpdate(PrepareStage stage, Run run, Note note) {
    for (unsigned attempt = 0; ; ++attempt) {
        CoreUpdateRestart output;
        auto result = run(output);
        if (!isCoreUpdateRestart(stage, result, output))
            return result;
        note("Confirmed MSYS2 core shutdown, original exit=" + std::to_string(result.code) +
             ". Restarting this update in a new Bash process.\r\n");
        if (attempt >= 2) {
            note("Core update requested three restarts without completing; cache retained.\r\n");
            result.code = 75;
            return result;
        }
    }
}
} // namespace relaxer::launch
