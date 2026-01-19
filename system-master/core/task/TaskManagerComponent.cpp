#include "TaskManagerComponent.h"
#include "../scheduler/Scheduler.h"

namespace system_runtime {

TaskManagerComponent::TaskManagerComponent(SystemBus& bus)
    : bus_(bus) {}

std::string_view TaskManagerComponent::name() const {
    return "TaskManagerComponent";
}

bool TaskManagerComponent::init() {
    auto scheduler = bus_.getService<Scheduler>();
    if (!scheduler) {
        return false;
    }

    manager_.bindScheduler(scheduler.get());

    bus_.registerService<TaskManager>(
        std::shared_ptr<TaskManager>(&manager_, [](TaskManager*) {})
    );

    return true;
}

bool TaskManagerComponent::start() {
    manager_.registerToScheduler();
    return true;
}

void TaskManagerComponent::stop() {
}

} // namespace system_runtime
