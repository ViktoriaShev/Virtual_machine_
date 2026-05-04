#include "logger_bridge.h"

#include "./legacy/include/vm32_api.h"
#include "../../core/interfaces/ILogger.h"

#include <atomic>

using namespace system_runtime;

/* Глобальный указатель на системный логгер (не владеет) */
static ILogger* g_system_log = nullptr;

/* Установка сырого указателя на логгер (вызывает VM32Component::init) */
extern "C" void vm32_bridge_set_system_logger(void* logger_ptr) {
    g_system_log = static_cast<ILogger*>(logger_ptr);
}

/* Callback, который мы передадим в C код (vm32_set_log_callback) */
extern "C" void vm32_bridge_log_cb(int level, const char* msg) {
    if (!g_system_log || !msg) return;
    switch (level) {
        case VM32_LOG_ERROR:
            g_system_log->error(msg);
            break;
        case VM32_LOG_DEBUG:
        case VM32_LOG_INFO:
        default:
            g_system_log->info(msg);
            break;
    }
}
