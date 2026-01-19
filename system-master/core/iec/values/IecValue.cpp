#include "IecValue.h"

namespace system_runtime {

IecValue::IecValue(const IecType& type)
    : type_(type) {}

const IecType& IecValue::type() const {
    return type_;
}

} // namespace system_runtime
