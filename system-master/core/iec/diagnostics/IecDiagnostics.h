#pragma once

#include "IecRuntimeState.h"

namespace system_runtime {

/*
 * IEC Diagnostics interface (skeleton).
 * Future:
 *  - error reporting
 *  - warnings
 *  - tracing
 */
class IecDiagnostics {
public:
    void onCycleStart();
    void onCycleEnd();
    void onError();

    IecRuntimeState& state();

private:
    IecRuntimeState state_;
};

} // namespace system_runtime
