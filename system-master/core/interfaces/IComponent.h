#pragma once

#include <string_view>

namespace system_runtime {

struct IComponent {
    virtual ~IComponent() = default;

    virtual std::string_view name() const = 0;

    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
};

} // namespace system_runtime
