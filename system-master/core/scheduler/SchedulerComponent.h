#pragma once

#include "../interfaces/IComponent.h"
#include "../../bus/SystemBus.h"
#include "Scheduler.h"

namespace system_runtime {

class SchedulerComponent : public IComponent {
public:
    explicit SchedulerComponent(SystemBus& bus);

    std::string_view name() const override;

    bool init() override;
    bool start() override;
    void stop() override;

    void tick();

private:
    SystemBus& bus_;
    Scheduler scheduler_;
};

} // namespace system_runtime
