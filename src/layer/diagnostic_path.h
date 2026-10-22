#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace neural_companion::detail {

inline constexpr char kDefaultDiagnosticLogPath[] = "neural_companion_passthrough.log";

// A required-capacity result is not a string length: Windows leaves the
// output buffer undefined when the environment value does not fit.
inline const char* ResolveDiagnosticLogPath(char (&buffer)[MAX_PATH]) noexcept {
    const DWORD length = GetEnvironmentVariableA("NC_LOG_PATH", buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH && buffer[0] != '\0') {
        return buffer;
    }
    return kDefaultDiagnosticLogPath;
}

} // namespace neural_companion::detail
