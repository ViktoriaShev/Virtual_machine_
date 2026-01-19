#include "IecTask.h"

namespace system_runtime {

IecTask::IecTask(Task& task)
    : task_(task) {}

Task& IecTask::task() {
    return task_;
}

} // namespace system_runtime
