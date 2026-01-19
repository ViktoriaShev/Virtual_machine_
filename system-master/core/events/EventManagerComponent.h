#pragma once

#include "../../bus/SystemBus.h"
#include "../interfaces/IComponent.h"
#include "EventManager.h"

namespace system_runtime {

class EventManagerComponent : public IComponent {
public:
    explicit EventManagerComponent(SystemBus& bus);

    std::string_view name() const override;

    bool init() override;
    bool start() override;
    void stop() override;

private:
    SystemBus& bus_;
    EventManager manager_;
};

} // namespace system_runtime
