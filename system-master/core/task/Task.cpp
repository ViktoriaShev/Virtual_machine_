#include "Task.h"

namespace system_runtime {

Task::Task(const char* name)
    : name_(name) {}

const char* Task::name() const {
    return name_;
}

void Task::add(ITickable* unit) {
    units_.push_back(unit);
}

void Task::tick() {
    for (auto* u : units_) {
        u->tick();
    }
}

} // namespace system_runtime
