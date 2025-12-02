#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_TIMERS 16

/* Общая структура для всех таймеров IEC */
typedef struct {
    bool enabled;           // Разрешён ли таймер
    bool input;             // Текущее состояние IN
    bool prev_input;        // Предыдущее IN (для фронтов)
    uint32_t preset_ms;     // PT — уставка
    uint32_t ET;            // Прошедшее время (IEC Elapsed Time)
    struct timespec start;  // Время старта
    bool output;            // Q — выход таймера
    bool timing;            // Фаза "идёт таймер"
} IEC_Timer;

/* Таймеры для TON/TOF/TP/R */
extern IEC_Timer ton_timers[MAX_TIMERS];
extern IEC_Timer tof_timers[MAX_TIMERS];
extern IEC_Timer tp_timers[MAX_TIMERS];
extern IEC_Timer tonr_timers[MAX_TIMERS];
extern IEC_Timer tofr_timers[MAX_TIMERS];

/* Инициализация всех таймеров */
void timers_init(void);

/* Вызывается раз в цикл VM */
void update_all_timers(void);

/* --- Простые API для инструкций VM --------------------------------- */
/* Устанавливают вход/pt и включают таймер (и сразу обновляют его),
   чтобы op_* мог установить вход и тут же прочитать Q. */
void ton_set(uint8_t id, bool in, uint32_t pt);
bool ton_Q(uint8_t id);

void tof_set(uint8_t id, bool in, uint32_t pt);
bool tof_Q(uint8_t id);

void tp_set(uint8_t id, bool in, uint32_t pt);
bool tp_Q(uint8_t id);

#endif
