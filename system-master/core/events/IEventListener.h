#pragma once

#include "Event.h"

namespace system_runtime {

struct IEventListener {
    virtual ~IEventListener() = default;
    virtual void onEvent(const Event& event) = 0;
};

} // namespace system_runtime
