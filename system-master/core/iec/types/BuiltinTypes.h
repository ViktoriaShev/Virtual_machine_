#pragma once

#include "IecType.h"

namespace system_runtime {

/*
 * Built-in IEC types (skeleton).
 * Instances are shared descriptors.
 */
struct BuiltinTypes {
    static const IecType BOOL;
    static const IecType INT;
    static const IecType REAL;
};

} // namespace system_runtime
