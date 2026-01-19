#pragma once

#include "../values/IecValueRef.h"

namespace system_runtime {

/*
 * IEC IO endpoint (skeleton).
 * Abstract source or sink of a value.
 */
class IecIoEndpoint {
public:
    virtual ~IecIoEndpoint() = default;

    virtual void read(IecValueRef target) = 0;
    virtual void write(IecValueRef source) = 0;
};

} // namespace system_runtime
