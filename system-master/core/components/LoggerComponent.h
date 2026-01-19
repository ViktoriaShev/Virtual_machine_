#pragma once

#include "../../bus/SystemBus.h"
#include "../interfaces/IComponent.h"
#include "../interfaces/ILogger.h"
#include "../../system/interfaces/ISystemLog.h"

#include <memory>

namespace system_runtime {

class LoggerComponent : public IComponent, public ILogger {
public:
    explicit LoggerComponent(SystemBus& bus);

    std::string_view name() const override;

    bool init() override;
    bool start() override;
    void stop() override;

    void info(std::string_view message) override;
    void error(std::string_view message) override;

private:
    SystemBus& bus_;
    std::shared_ptr<ISystemLog> systemLog_;
};

} // namespace system_runtime
