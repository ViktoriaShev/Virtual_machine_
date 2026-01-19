#pragma once

#include <cstdint>

namespace system_runtime {

/*
 * RuntimeContext
 * Holds execution-scoped metadata.
 */
class RuntimeContext {
public:
    using CorrelationId = std::uint64_t;
    using CycleId = std::uint64_t;

    void newCycle();

    CorrelationId correlationId() const;
    CycleId cycleId() const;

private:
    CorrelationId correlationId_ = 0;
    CycleId cycleId_ = 0;
};

} // namespace system_runtime
