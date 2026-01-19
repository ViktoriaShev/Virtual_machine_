#pragma once

#include "../interfaces/IComponent.h"
#include "../../bus/SystemBus.h"
#include "TaskManager.h"

namespace system_runtime {

class TaskManagerComponent : public IComponent {
public:
    explicit TaskManagerComponent(SystemBus& bus);

    std::string_view name() const override;

    bool init() override;
    bool start() override;
    void stop() override;

private:
    SystemBus& bus_;
    TaskManager manager_;
};

} // namespace system_runtime
