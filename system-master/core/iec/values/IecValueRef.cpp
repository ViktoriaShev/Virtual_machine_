#include "IecValueRef.h"

namespace system_runtime {

IecValueRef::IecValueRef(IecValue* value)
    : value_(value) {}

IecValue* IecValueRef::get() const {
    return value_;
}

} // namespace system_runtime
