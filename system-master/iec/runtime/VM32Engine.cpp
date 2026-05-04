#include "VM32Engine.h"
#include <vector>
#include <cstring>

namespace system_runtime {

VM32Engine::VM32Engine(): initialized_(false) {}
VM32Engine::~VM32Engine() { if (initialized_) shutdown(); }

bool VM32Engine::init(const vm_config_t &cfg, const std::vector<std::string> &programFiles) {
    if (vm32_init(&cfg) != 0) return false;
    if (!programFiles.empty()) {
        loadPrograms(programFiles);
    }
    initialized_ = true;
    return true;
}

bool VM32Engine::loadPrograms(const std::vector<std::string> &programFiles) {
    std::vector<const char*> cfiles;
    cfiles.reserve(programFiles.size());
    for (auto &s: programFiles) cfiles.push_back(s.c_str());
    if (vm32_load_programs(cfiles.data(), (int)cfiles.size()) != 0)
    return false;

    return true;
}

void VM32Engine::executeCycle() { vm32_execute_cycle(); }
void VM32Engine::shutdown() { vm32_shutdown(); initialized_ = false; }
void VM32Engine::requestStop() { vm32_request_stop(); }

}
