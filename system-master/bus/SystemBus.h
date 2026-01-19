#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>

namespace system_runtime {

class SystemBus {
public:
    SystemBus() = default;

    template<typename T>
    void registerService(std::shared_ptr<T> service) {
        services_[std::type_index(typeid(T))] = service;
    }

    template<typename T>
    std::shared_ptr<T> getService() {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it == services_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second);
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
};

} // namespace system_runtime
