#include "IecVariable.h"

namespace system_runtime {

IecVariable::IecVariable(std::string_view name, const IecType& type)
    : name_(name), type_(type) {}

std::string_view IecVariable::name() const {
    return name_;
}

const IecType& IecVariable::type() const {
    return type_;
}

void IecVariable::bindValue(IecValueRef value) {
    value_ = value;
}

IecValueRef IecVariable::value() const {
    return value_;
}

} // namespace system_runtime
