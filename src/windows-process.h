#pragma once
#include "launch-plan.h"
#include <functional>
#include <vector>
#include <windows.h>

namespace relaxer::launch {
struct NativeHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit NativeHandle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
    ~NativeHandle() { close(); }
    void close() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
        value = INVALID_HANDLE_VALUE;
    }
    NativeHandle(const NativeHandle &) = delete;
    NativeHandle &operator=(const NativeHandle &) = delete;
};
struct ProcessResult {
    bool started = false;
    DWORD code = 0, systemError = 0, readError = 0;
};
// Arguments are values, not pre-quoted shell fragments. Invoke the specified
// executable directly and drain output before waiting, preserving its status.
inline ProcessResult capture(const fs::path &exe, const std::vector<std::wstring> &arguments,
                             const fs::path &directory,
                             const std::function<void(const char *, DWORD)> &output,
                             DWORD timeoutMs = 7200000) {
    ProcessResult result;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE reader = nullptr, writer = nullptr;
    if (!CreatePipe(&reader, &writer, &security, 0)) {
        result.systemError = GetLastError();
        return result;
    }
    NativeHandle inputPipe(reader), outputPipe(writer);
    if (!SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0)) {
        result.systemError = GetLastError();
        return result;
    }
    NativeHandle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                   OPEN_EXISTING, 0, nullptr));
    if (input.value == INVALID_HANDLE_VALUE) {
        result.systemError = GetLastError();
        return result;
    }
    auto application = exe;
    auto workingDirectory = directory;
    application.make_preferred();
    workingDirectory.make_preferred();
    std::wstring command = quote(application.wstring());
    for (auto &argument : arguments)
        command += L" " + quote(argument);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.value;
    startup.hStdOutput = startup.hStdError = writer;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(application.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, workingDirectory.c_str(), &startup, &process)) {
        result.systemError = GetLastError();
        return result;
    }
    result.started = true;
    NativeHandle thread(process.hThread), child(process.hProcess);
    outputPipe.close();
    char buffer[8192];
    DWORD count = 0;
    const auto startedAt = GetTickCount64();
    for (;;) {
        // Background package helpers may inherit stdout and outlive the main
        // process. Drain available output without waiting forever for their EOF.
        DWORD available = 0;
        if (!PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr)) {
            DWORD pipeError = GetLastError();
            if (pipeError != ERROR_BROKEN_PIPE)
                result.readError = pipeError;
            break;
        }
        if (available) {
            if (!ReadFile(reader, buffer, available < sizeof(buffer) ? available : sizeof(buffer), &count,
                          nullptr)) {
                if (GetLastError() != ERROR_BROKEN_PIPE)
                    result.readError = GetLastError();
                break;
            }
            if (count)
                output(buffer, count);
        } else if (WaitForSingleObject(child.value, 0) == WAIT_OBJECT_0)
            break;
        else
            Sleep(25);
        if (GetTickCount64() - startedAt >= timeoutMs) {
            result.systemError = ERROR_TIMEOUT;
            TerminateProcess(child.value, ERROR_TIMEOUT);
            break;
        }
    }
    const auto elapsed = GetTickCount64() - startedAt;
    const auto remaining = elapsed >= timeoutMs ? 0 : timeoutMs - DWORD(elapsed);
    const auto waited = WaitForSingleObject(child.value, remaining);
    if (waited == WAIT_TIMEOUT) {
        result.systemError = ERROR_TIMEOUT;
        TerminateProcess(child.value, ERROR_TIMEOUT);
        WaitForSingleObject(child.value, 5000);
    } else if (waited == WAIT_FAILED)
        result.systemError = GetLastError();
    if (!GetExitCodeProcess(child.value, &result.code))
        result.systemError = GetLastError();
    return result;
}
} // namespace relaxer::launch
