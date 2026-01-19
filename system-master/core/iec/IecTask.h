#pragma once

#include "../task/Task.h"

namespace system_runtime {

/*
 * IEC Task — логическая оболочка над Task.
 * В будущем:
 *  - period
 *  - priority
 *  - watchdog
 */
class IecTask {
public:
    explicit IecTask(Task& task);

    Task& task();

private:
    Task& task_;
};

} // namespace system_runtime
