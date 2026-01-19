#pragma once

#include "../interfaces/ITickable.h"
#include "../runtime/RuntimeContext.h"
#include "memory/IecMemory.h"
#include "execution/IecExecutionEngine.h"
#include "execution/IecExecutionContext.h"
#include "diagnostics/IecDiagnostics.h"
#include "io/IecIoBinding.h"

namespace system_runtime {

class IecProgram : public ITickable {
public:
    explicit IecProgram(const char* name);

    const char* name() const;

    IecMemory& memory();
    IecIoBinding& io();

    void addExpression(IecExpression* expr);
    void tick() override;

private:
    const char* name_;
    RuntimeContext runtime_;
    IecMemory memory_;
    IecDiagnostics diagnostics_;
    IecExecutionEngine engine_;
    IecExecutionContext context_;
    IecIoBinding io_;
};

} // namespace system_runtime
