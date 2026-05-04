#pragma once
#include <vector>
#include <string>
#include "./legacy/include/vm32_api.h"

namespace system_runtime {

class VM32Engine {
public:
    VM32Engine();
    ~VM32Engine();

    // init: передаём config и список файлов (можно пустой)
    bool init(const vm_config_t &cfg, const std::vector<std::string> &programFiles);
    bool loadPrograms(const std::vector<std::string> &programFiles);
    void executeCycle();   // вызывает vm32_execute_cycle()
    void shutdown();
    void requestStop();

private:
    bool initialized_;
};

}
