cmake_minimum_required(VERSION 3.16)

project(system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

add_library(core
    core/component_manager/ComponentManager.cpp
    core/components/LoggerComponent.cpp
    core/events/EventManagerComponent.cpp
    core/scheduler/Scheduler.cpp
    core/scheduler/SchedulerComponent.cpp
    core/task/Task.cpp
    core/task/TaskManager.cpp
    core/task/TaskManagerComponent.cpp
    core/runtime/RuntimeContext.cpp
    core/logging/StructuredLogger.cpp
    core/iec/IecProgram.cpp
    core/iec/IecTask.cpp
    core/iec/memory/IecVariable.cpp
    core/iec/memory/IecMemory.cpp
    core/iec/types/IecType.cpp
    core/iec/types/BuiltinTypes.cpp
    core/iec/values/IecValue.cpp
    core/iec/values/IecValueRef.cpp
    core/iec/expressions/IecAssignment.cpp
    core/iec/execution/IecExecutionContext.cpp
    core/iec/execution/IecExecutionEngine.cpp
    core/iec/diagnostics/IecRuntimeState.cpp
    core/iec/diagnostics/IecDiagnostics.cpp
    core/iec/io/IecIoBinding.cpp
    system/device/DeviceManager.cpp
    system/device/NullDevice.cpp
    system/fieldbus/FieldbusManager.cpp
    system/fieldbus/NullFieldbus.cpp
    system/config/SystemConfig.cpp
    system/config/ConfigApplier.cpp
    system/lifecycle/LifecycleController.cpp
    system/posix/PosixLog.cpp
    vm32legacy STATIC
    legacy/vm32.c
    legacy/funcs.c
    legacy/timers.c
    legacy/hashing.c
    legacy/debug.c
    legacy/cleanup.c
    legacy/converter.c
)

target_include_directories(vm32legacy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/legacy/include)
target_link_libraries(vm32legacy PUBLIC m) 

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(system
    main.cpp
)

target_link_libraries(system PRIVATE core)
