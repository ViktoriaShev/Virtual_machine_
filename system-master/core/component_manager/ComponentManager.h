#pragma once

#include <memory>
#include <vector>

#include "../interfaces/IComponent.h"
#include "../../bus/SystemBus.h"

namespace system_runtime {

class ComponentManager {
public:
    explicit ComponentManager(SystemBus& bus);

    void addComponent(std::shared_ptr<IComponent> component);

    bool initAll();
    bool startAll();
    void stopAll();

private:
    SystemBus& bus_;
    std::vector<std::shared_ptr<IComponent>> components_;
};

} // namespace system_runtime
