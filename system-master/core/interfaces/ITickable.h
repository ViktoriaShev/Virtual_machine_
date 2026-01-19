#pragma once

namespace system_runtime {

struct ITickable {
    virtual ~ITickable() = default;
    virtual void tick() = 0;
};

} // namespace system_runtime
