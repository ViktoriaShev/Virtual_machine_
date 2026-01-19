#include "RuntimeContext.h"

namespace system_runtime {

void RuntimeContext::newCycle() {
    ++cycleId_;
    ++correlationId_;
}

RuntimeContext::CorrelationId RuntimeContext::correlationId() const {
    return correlationId_;
}

RuntimeContext::CycleId RuntimeContext::cycleId() const {
    return cycleId_;
}

} // namespace system_runtime
