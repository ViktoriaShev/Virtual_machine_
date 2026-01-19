#include "IecExecutionContext.h"
#include "../../logging/StructuredLogger.h"
#include "../../../bus/SystemBus.h"

namespace system_runtime {

IecExecutionContext::IecExecutionContext(
    IecDiagnostics& diag,
    RuntimeContext& rt
)
    : diagnostics_(diag), runtime_(rt) {}

void IecExecutionContext::beginCycle() {
    runtime_.newCycle();
    diagnostics_.onCycleStart();
}

void IecExecutionContext::endCycle() {
    diagnostics_.onCycleEnd();
}

const RuntimeContext& IecExecutionContext::runtime() const {
    return runtime_;
}

} // namespace system_runtime
