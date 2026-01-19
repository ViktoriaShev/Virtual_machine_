#pragma once

#include <string_view>

namespace system_runtime {

/*
 * Base IEC type descriptor (skeleton).
 * No value storage, only type identity.
 */
class IecType {
public:
    explicit IecType(std::string_view name);

    std::string_view name() const;

private:
    std::string_view name_;
};

} // namespace system_runtime
