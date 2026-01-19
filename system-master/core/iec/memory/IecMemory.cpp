#include "IecMemory.h"

namespace system_runtime {

void IecMemory::addVar(IecVariable var) {
    vars_.push_back(var);
}

void IecMemory::addInput(IecVariable var) {
    inputs_.push_back(var);
}

void IecMemory::addOutput(IecVariable var) {
    outputs_.push_back(var);
}

const std::vector<IecVariable>& IecMemory::vars() const {
    return vars_;
}

const std::vector<IecVariable>& IecMemory::inputs() const {
    return inputs_;
}

const std::vector<IecVariable>& IecMemory::outputs() const {
    return outputs_;
}

} // namespace system_runtime
