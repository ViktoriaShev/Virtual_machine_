#include "Metrics.h"

namespace system_runtime {

void Metrics::onCycle() {
    ++cycleCount_;
}

std::uint64_t Metrics::cycles() const {
    return cycleCount_;
}

} // namespace system_runtime
