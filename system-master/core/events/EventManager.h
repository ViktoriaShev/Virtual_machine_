#pragma once

#include <unordered_map>
#include <vector>
#include <typeindex>

#include "IEventListener.h"

namespace system_runtime {

class EventManager {
public:
    EventManager() = default;

    template<typename EventType>
    void subscribe(IEventListener* listener) {
        listeners_[std::type_index(typeid(EventType))].push_back(listener);
    }

    template<typename EventType>
    void publish(const EventType& event) {
        auto it = listeners_.find(std::type_index(typeid(EventType)));
        if (it == listeners_.end()) {
            return;
        }

        for (auto* listener : it->second) {
            listener->onEvent(event);
        }
    }

private:
    std::unordered_map<
        std::type_index,
        std::vector<IEventListener*>
    > listeners_;
};

} // namespace system_runtime
