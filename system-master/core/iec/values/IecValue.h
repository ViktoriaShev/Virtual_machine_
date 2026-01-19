#pragma once

#include "../types/IecType.h"

namespace system_runtime {

/*
 * Base IEC value storage (skeleton).
 * Does not define concrete representation.
 */
class IecValue {
public:
    explicit IecValue(const IecType& type);
    virtual ~IecValue() = default;

    const IecType& type() const;

private:
    const IecType& type_;
};

} // namespace system_runtime
