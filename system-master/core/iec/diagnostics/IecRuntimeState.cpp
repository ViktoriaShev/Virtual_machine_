#include "IecRuntimeState.h"

namespace system_runtime {

void IecRuntimeState::setStatus(Status status) {
    status_ = status;
}

IecRuntimeState::Status IecRuntimeState::status() const {
    return status_;
}

} // namespace system_runtime
