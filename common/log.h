#pragma once

#include <string>

namespace logging {
    // Opens the log file at path (truncated on each game launch).
    void Init(const std::string& path);
    void Write(const char* fmt, ...);
}

#define LOG(fmt, ...) logging::Write(fmt, __VA_ARGS__)
