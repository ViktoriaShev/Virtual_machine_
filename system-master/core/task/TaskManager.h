#pragma once

#include <vector>
#include <memory>

#include "Task.h"

namespace system_runtime {

class Scheduler;  // forward declaration

class TaskManager {
public:
    TaskManager() = default;

    Task& createTask(const char* name);

    void bindScheduler(Scheduler* scheduler);
    void registerToScheduler();

private:
    std::vector<std::unique_ptr<Task>> tasks_;
    Scheduler* scheduler_ = nullptr;
};

} // namespace system_runtime
