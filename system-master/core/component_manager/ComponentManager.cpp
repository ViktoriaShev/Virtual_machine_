#include "ComponentManager.h"

namespace system_runtime {

ComponentManager::ComponentManager(SystemBus& bus)
    : bus_(bus) {
}

void ComponentManager::addComponent(std::shared_ptr<IComponent> component) {
    components_.push_back(std::move(component));
}

bool ComponentManager::initAll() {
    for (auto& c : components_) {
        if (!c->init()) {
            return false;
        }
    }
    return true;
}

bool ComponentManager::startAll() {
    for (auto& c : components_) {
        if (!c->start()) {
            return false;
        }
    }
    return true;
}

void ComponentManager::stopAll() {
    for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
        (*it)->stop();
    }
}

} // namespace system_runtime
