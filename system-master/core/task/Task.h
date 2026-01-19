#pragma once

#include <vector>
#include "../interfaces/ITickable.h"

namespace system_runtime {

class Task : public ITickable {
public:
    explicit Task(const char* name);

    const char* name() const;

    void add(ITickable* unit);
    void tick() override;

private:
    const char* name_;
    std::vector<ITickable*> units_;
};

} // namespace system_runtime
