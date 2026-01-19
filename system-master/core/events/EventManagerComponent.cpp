#include "EventManagerComponent.h"

namespace system_runtime {

EventManagerComponent::EventManagerComponent(SystemBus& bus)
    : bus_(bus) {}

std::string_view EventManagerComponent::name() const {
    return "EventManagerComponent";
}

bool EventManagerComponent::init() {
    bus_.registerService<EventManager>(
        std::shared_ptr<EventManager>(&manager_, [](EventManager*) {})
    );
    return true;
}

bool EventManagerComponent::start() {
    return true;
}

void EventManagerComponent::stop() {
}

} // namespace system_runtime
