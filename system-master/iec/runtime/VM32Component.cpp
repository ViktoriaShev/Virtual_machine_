#include "VM32Component.h"
#include "../../bus/SystemBus.h"
#include "../../core/task/TaskManager.h"
#include "../../core/interfaces/ILogger.h"
#include "logger_bridge.h"
#include <iostream>
#include <memory>

namespace system_runtime {

    VM32Component::VM32Component() {
        config_.cycle_time_ms = 1000;
        config_.clock_rate_hz = 100;
        config_.enable_hash_check = true;
        config_.enable_cycle_check = true;
        config_.enable_tick_timing = false;
        config_.hash_algo = HASH_CRC32;
    }

    VM32Component::~VM32Component() {
        stop();
    }

    void VM32Component::setBus(SystemBus* bus) {
        bus_ = bus;
    }

    void VM32Component::setProgramFiles(std::vector<std::string> files) {
        programFiles_ = std::move(files);
    }

    bool VM32Component::init() {
        if (!bus_) {
            std::cerr << "VM32Component: SystemBus not set\n";
            return false;
        }
        auto sysLogPtr = bus_->getService<ILogger>();
        if (!sysLogPtr) {
            std::cerr << "VM32Component: ISystemLog not available\n";
        } else {
            // Установим указатель на системный логгер в bridge и передадим callback
            vm32_bridge_set_system_logger(static_cast<void*>(sysLogPtr.get()));
            vm32_set_log_callback(&vm32_bridge_log_cb);
        }

        if (!engine_.init(config_, programFiles_)) {
            std::cerr << "VM32Component: VM init failed\n";
            return false;
        }

        return true;
    }

    bool VM32Component::start() {
        if (!bus_) {
            std::cerr << "VM32Component::start: SystemBus not set\n";
            return false;
        }
        auto tm = bus_->getService<TaskManager>();
        if (!tm) {
            std::cerr << "VM32Component: TaskManager not found\n";
            return false;
        }

        // создаём Task, а не регистрируем ITickable напрямую
        Task& task = tm->createTask("vm32-cycle");

    // КЛЮЧЕВО: добавляем себя как ITickable
        task.add(this);

        return true;
    }

    void VM32Component::stop() {
        engine_.requestStop();
        engine_.shutdown();
    }

    void VM32Component::tick() {
        engine_.executeCycle();
    }

} 
