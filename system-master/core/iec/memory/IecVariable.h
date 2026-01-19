#pragma once

#include <string_view>
#include "../types/IecType.h"
#include "../values/IecValueRef.h"

namespace system_runtime {

/*
 * IEC variable skeleton with type and value reference.
 */
class IecVariable {
public:
    IecVariable(std::string_view name, const IecType& type);

    std::string_view name() const;
    const IecType& type() const;

    void bindValue(IecValueRef value);
    IecValueRef value() const;

private:
    std::string_view name_;
    const IecType& type_;
    IecValueRef value_{nullptr};
};

} // namespace system_runtime
