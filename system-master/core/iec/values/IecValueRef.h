#pragma once

#include "IecValue.h"

namespace system_runtime {

/*
 * Reference to IEC value storage.
 * Used by variables to access values.
 */
class IecValueRef {
public:
    explicit IecValueRef(IecValue* value);

    IecValue* get() const;

private:
    IecValue* value_;
};

} // namespace system_runtime
