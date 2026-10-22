#include "diagnostic_path.h"

#include <cstring>
#include <iostream>
#include <string>

namespace {

bool SetValue(const char* value) {
    // Only this disposable test process's environment changes. Never query or
    // print a pre-existing value, enumerate the host environment, or open logs.
    if (!SetEnvironmentVariableA("NC_LOG_PATH", value)
        && !(value == nullptr && GetLastError() == ERROR_ENVVAR_NOT_FOUND)) {
        std::cerr << "FAIL: synthetic environment setup\n";
        return false;
    }
    return true;
}

bool Check(const char* value, bool expectDefault) {
    if (!SetValue(value)) return false;
    char buffer[MAX_PATH];
    for (char& byte : buffer) byte = 'X';
    const char* selected = neural_companion::detail::ResolveDiagnosticLogPath(buffer);
    if (expectDefault) {
        return selected == neural_companion::detail::kDefaultDiagnosticLogPath
            && std::strcmp(selected, "neural_companion_passthrough.log") == 0;
    }
    return selected == buffer && value != nullptr && std::strcmp(selected, value) == 0;
}

bool CheckStaleBuffer() {
    char buffer[MAX_PATH];
    const std::string longValue(MAX_PATH + 32, 'L');
    if (!SetValue("synthetic-first.log")) return false;
    if (neural_companion::detail::ResolveDiagnosticLogPath(buffer) != buffer) return false;
    if (std::strcmp(buffer, "synthetic-first.log") != 0) return false;
    if (!SetValue(longValue.c_str())) return false;
    if (neural_companion::detail::ResolveDiagnosticLogPath(buffer)
        != neural_companion::detail::kDefaultDiagnosticLogPath) return false;
    if (!SetValue("synthetic-next.log")) return false;
    return neural_companion::detail::ResolveDiagnosticLogPath(buffer) == buffer
        && std::strcmp(buffer, "synthetic-next.log") == 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string name = argv[1];
    bool passed = false;
    if (name == "Unset") passed = Check(nullptr, true);
    else if (name == "Empty") passed = Check("", true);
    else if (name == "Short") passed = Check("synthetic-diagnostic.log", false);
    else if (name == "MaximumFit") passed = Check(std::string(MAX_PATH - 1, 'F').c_str(), false);
    else if (name == "ExactCapacity") passed = Check(std::string(MAX_PATH, 'E').c_str(), true);
    else if (name == "OneOverCapacity") passed = Check(std::string(MAX_PATH + 1, 'O').c_str(), true);
    else if (name == "LargeValue") passed = Check(std::string(4096, 'B').c_str(), true);
    else if (name == "StaleAfterLong") passed = CheckStaleBuffer();
    else return 2;

    // Fixed output only: never print selected paths, environment values or host data.
    std::cout << (passed ? "PASS\n" : "FAIL\n");
    return passed ? 0 : 1;
}
