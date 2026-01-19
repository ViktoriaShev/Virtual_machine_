#pragma once

#include "../diagnostics/IecDiagnostics.h"
#include "../../runtime/RuntimeContext.h"

namespace system_runtime {

/*
 * IEC Execution Context with runtime metadata.
 */
class IecExecutionContext {
public:
    IecExecutionContext(IecDiagnostics& diag, RuntimeContext& rt);

    void beginCycle();
    void endCycle();

    const RuntimeContext& runtime() const;

private:
    IecDiagnostics& diagnostics_;
    RuntimeContext& runtime_;
};

} // namespace system_runtime
