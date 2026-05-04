#pragma once

#include <vector>
#include <string>

#include "VM32Engine.h"
#include "../../core/interfaces/IComponent.h"
#include "../../core/interfaces/ITickable.h"

namespace system_runtime {

    class SystemBus;
    class TaskManager;

    class VM32Component final: public IComponent, public ITickable {
        public:
            VM32Component();
            ~VM32Component() override;

            std::string_view name() const override { return "VM32Component"; }

            bool init() override;
            bool start() override;
            void stop() override;

            // ITickable
            void tick() override;

            void setBus(SystemBus* bus);
            void setProgramFiles(std::vector<std::string> files);

        private:
            VM32Engine engine_;
            std::vector<std::string> programFiles_;
            SystemBus* bus_ = nullptr;
            vm_config_t config_;
        };


} 
