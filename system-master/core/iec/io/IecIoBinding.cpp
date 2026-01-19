#include "IecIoBinding.h"

namespace system_runtime {

void IecIoBinding::addInput(IecIoEndpoint* endpoint, IecValueRef target) {
    inputs_.push_back({endpoint, target});
}

void IecIoBinding::addOutput(IecIoEndpoint* endpoint, IecValueRef source) {
    outputs_.push_back({endpoint, source});
}

void IecIoBinding::readInputs() {
    for (auto& b : inputs_) {
        b.endpoint->read(b.target);
    }
}

void IecIoBinding::writeOutputs() {
    for (auto& b : outputs_) {
        b.endpoint->write(b.source);
    }
}

} // namespace system_runtime
