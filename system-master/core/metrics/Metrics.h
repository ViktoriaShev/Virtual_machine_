#pragma once

#include <cstdint>

namespace system_runtime {

/*
 * Minimal metrics counters.
 */
class Metrics {
public:
    void onCycle();
    std::uint64_t cycles() const;

private:
    std::uint64_t cycleCount_ = 0;
};

} // namespace system_runtime
