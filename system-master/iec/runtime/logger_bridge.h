#pragma once

extern "C" {
void vm32_bridge_set_system_logger(void* logger_ptr);
void vm32_bridge_log_cb(int level, const char* msg);
}
