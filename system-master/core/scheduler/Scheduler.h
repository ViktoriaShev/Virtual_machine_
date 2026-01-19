#pragma once

#include <vector>

#include "../interfaces/ITickable.h"

namespace system_runtime {

class Scheduler {
public:
    void addTask(ITickable* task);
    void tick();

private:
    std::vector<ITickable*> tasks_;
};

} // namespace system_runtime
