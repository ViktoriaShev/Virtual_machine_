#ifndef VM32_API_H
#define VM32_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* log levels для мостов */
#define VM32_LOG_INFO  0
#define VM32_LOG_ERROR 1
#define VM32_LOG_DEBUG 2


#include "vm32.h" /* vm_config_t, REG_COUNT и т.д. */

/* Инициализация VM (выделение памяти, init timers/tables и т.д.)
   Передаём NULL, чтобы использовать vm_config (дефолт) */
int vm32_init(const vm_config_t *cfg);

/* Загрузка программ (копирует имена внутрь VM и вызывает load_programs) */
int vm32_load_programs(const char **fnames, int count);

/* Выполнение ровно одного PLC-цикла (без sleep/usleep) */
void vm32_execute_cycle(void);

/* Запрос на корректную остановку (как сигнал) */
void vm32_request_stop(void);

/* Возвращает 1 если vm выполняется (не остановлен запросом), 0 иначе */
int vm32_is_running(void);

/* Отключение и очистка ресурсов (vm_tables_destroy, free(mem), free modules) */
void vm32_shutdown(void);

typedef void (*vm32_log_cb_t)(int level, const char *msg);

/* Установить обратный вызов для доставки логов в host */
void vm32_set_log_callback(vm32_log_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* VM32_API_H */
