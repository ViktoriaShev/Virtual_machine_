#include "TaskManager.h"
#include "../scheduler/Scheduler.h"

namespace system_runtime {

Task& TaskManager::createTask(const char* name) {
    tasks_.push_back(std::make_unique<Task>(name));
    return *tasks_.back();
}

void TaskManager::bindScheduler(Scheduler* scheduler) {
    scheduler_ = scheduler;
}

void TaskManager::registerToScheduler() {
    if (!scheduler_) {
        return;
    }

    for (auto& task : tasks_) {
        scheduler_->addTask(task.get());
    }
}

} // namespace system_runtime
