#include "StructuredLogger.h"

namespace system_runtime {

StructuredLogger::StructuredLogger(
    ISystemLog& log,
    const RuntimeContext& ctx
)
    : log_(log), ctx_(ctx) {}

void StructuredLogger::info(std::string_view message) const {
    log_.info(format(message));
}

void StructuredLogger::error(std::string_view message) const {
    log_.error(format(message));
}

std::string StructuredLogger::format(std::string_view message) const {
    return
        "cid=" + std::to_string(ctx_.correlationId()) +
        " cycle=" + std::to_string(ctx_.cycleId()) +
        " | " + std::string(message);
}

} // namespace system_runtime
