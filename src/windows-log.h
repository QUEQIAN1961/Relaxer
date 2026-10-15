#pragma once
#include <string>
#include <windows.h>

namespace relaxer::launch {
// Bash/build output is UTF-8; CMD can report a failure in the system OEM
// encoding before the preparation script has changed the console code page.
inline std::wstring decodeLogLine(const std::string &bytes, UINT fallbackPage = CP_OEMCP) {
    if (bytes.empty())
        return {};
    UINT page = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int count = MultiByteToWideChar(page, flags, bytes.data(), int(bytes.size()), nullptr, 0);
    if (!count) {
        page = fallbackPage;
        flags = 0;
        count = MultiByteToWideChar(page, flags, bytes.data(), int(bytes.size()), nullptr, 0);
    }
    std::wstring result(count, L'\0');
    if (count)
        MultiByteToWideChar(page, flags, bytes.data(), int(bytes.size()), result.data(), count);
    return result;
}
// Decode complete lines separately: a log may contain both a CMD error in
// OEM encoding and UTF-8 diagnostics from later child processes.
inline std::wstring decodeLog(const std::string &bytes, UINT fallbackPage = CP_OEMCP) {
    std::wstring result;
    size_t start = 0;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start);
        if (end == std::string::npos)
            end = bytes.size();
        result += decodeLogLine(bytes.substr(start, end - start), fallbackPage);
        if (end < bytes.size())
            result += L'\n';
        start = end + 1;
    }
    return result;
}
} // namespace relaxer::launch
