#pragma once

#include <string_view>

namespace system_runtime {

struct ILogger {
    virtual ~ILogger() = default;

    virtual void info(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
};

} // namespace system_runtime
