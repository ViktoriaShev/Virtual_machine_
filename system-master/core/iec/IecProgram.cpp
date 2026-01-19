#include "IecProgram.h"

namespace system_runtime {

IecProgram::IecProgram(const char* name)
    : name_(name),
      runtime_(),
      diagnostics_(),
      engine_(),
      context_(diagnostics_, runtime_),
      io_() {}

const char* IecProgram::name() const {
    return name_;
}

IecMemory& IecProgram::memory() {
    return memory_;
}

IecIoBinding& IecProgram::io() {
    return io_;
}

void IecProgram::addExpression(IecExpression* expr) {
    engine_.addExpression(expr);
}

void IecProgram::tick() {
    io_.readInputs();
    engine_.executeCycle(context_);
    io_.writeOutputs();
}

} // namespace system_runtime
