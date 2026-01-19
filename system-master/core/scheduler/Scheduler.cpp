#include "Scheduler.h"

namespace system_runtime {

void Scheduler::addTask(ITickable* task) {
    tasks_.push_back(task);
}

void Scheduler::tick() {
    for (auto* task : tasks_) {
        task->tick();
    }
}

} // namespace system_runtime
