#include "SchedulerComponent.h"

namespace system_runtime {

SchedulerComponent::SchedulerComponent(SystemBus& bus)
    : bus_(bus) {}

std::string_view SchedulerComponent::name() const {
    return "SchedulerComponent";
}

bool SchedulerComponent::init() {
    bus_.registerService<Scheduler>(
        std::shared_ptr<Scheduler>(&scheduler_, [](Scheduler*) {})
    );
    return true;
}

bool SchedulerComponent::start() {
    return true;
}

void SchedulerComponent::stop() {
}

void SchedulerComponent::tick() {
    scheduler_.tick();
}

} // namespace system_runtime
