#include "IecType.h"

namespace system_runtime {

IecType::IecType(std::string_view name)
    : name_(name) {}

std::string_view IecType::name() const {
    return name_;
}

} // namespace system_runtime
