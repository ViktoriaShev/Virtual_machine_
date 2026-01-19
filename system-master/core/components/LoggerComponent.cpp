#include "LoggerComponent.h"

namespace system_runtime {

LoggerComponent::LoggerComponent(SystemBus& bus)
    : bus_(bus) {}

std::string_view LoggerComponent::name() const {
    return "LoggerComponent";
}

bool LoggerComponent::init() {
    systemLog_ = bus_.getService<ISystemLog>();
    if (!systemLog_) {
        return false;
    }

    bus_.registerService<ILogger>(
        std::shared_ptr<ILogger>(this, [](ILogger*) {})
    );

    return true;
}

bool LoggerComponent::start() {
    systemLog_->info("LoggerComponent started");
    return true;
}

void LoggerComponent::stop() {
    systemLog_->info("LoggerComponent stopped");
}

void LoggerComponent::info(std::string_view message) {
    systemLog_->info(message);
}

void LoggerComponent::error(std::string_view message) {
    systemLog_->error(message);
}

} // namespace system_runtime
