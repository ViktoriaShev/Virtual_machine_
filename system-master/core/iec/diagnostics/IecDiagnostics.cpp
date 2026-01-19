#include "IecDiagnostics.h"

namespace system_runtime {

void IecDiagnostics::onCycleStart() {
    state_.setStatus(IecRuntimeState::Status::Running);
}

void IecDiagnostics::onCycleEnd() {
    state_.setStatus(IecRuntimeState::Status::Idle);
}

void IecDiagnostics::onError() {
    state_.setStatus(IecRuntimeState::Status::Error);
}

IecRuntimeState& IecDiagnostics::state() {
    return state_;
}

} // namespace system_runtime
