#include "bus/SystemBus.h"

#include "core/component_manager/ComponentManager.h"
#include "core/components/LoggerComponent.h"
#include "core/events/EventManagerComponent.h"
#include "core/scheduler/SchedulerComponent.h"
#include "core/task/TaskManagerComponent.h"

#include "system/lifecycle/LifecycleController.h"
#include "system/posix/PosixLog.h"

using namespace system_runtime;

int main() {
    SystemBus bus;
    LifecycleController lifecycle;

    auto log = std::make_shared<PosixLog>();
    bus.registerService<ISystemLog>(log);

    ComponentManager cm(bus);

    auto eventMgr = std::make_shared<EventManagerComponent>(bus);
    auto logger   = std::make_shared<LoggerComponent>(bus);
    auto sched    = std::make_shared<SchedulerComponent>(bus);
    auto taskMgr  = std::make_shared<TaskManagerComponent>(bus);

    cm.addComponent(eventMgr);
    cm.addComponent(logger);
    cm.addComponent(sched);
    cm.addComponent(taskMgr);

    if (!lifecycle.init()) return 1;
    if (!cm.initAll()) return 1;
    if (!lifecycle.start()) return 1;
    if (!cm.startAll()) return 1;

    lifecycle.stop();
    cm.stopAll();
    lifecycle.shutdown();

    return 0;
}
