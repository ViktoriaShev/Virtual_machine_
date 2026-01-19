#pragma once

#include <string>
#include <string_view>

#include "../runtime/RuntimeContext.h"
#include "../../system/interfaces/ISystemLog.h"

namespace system_runtime {

/*
 * Structured logging adapter.
 * Does NOT replace ISystemLog.
 */
class StructuredLogger {
public:
    StructuredLogger(ISystemLog& log, const RuntimeContext& ctx);

    void info(std::string_view message) const;
    void error(std::string_view message) const;

private:
    ISystemLog& log_;
    const RuntimeContext& ctx_;

    std::string format(std::string_view message) const;
};

} // namespace system_runtime
