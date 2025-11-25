
#ifndef VM_H
#define VM_H
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static inline uint64_t current_millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


// получение опкода
#define OPC(i) ((i) >> 25)
#define RA(i) (((i) >> 17) & 0xFF)
#define RB(i) (((i) >> 9) & 0xFF)
#define RC(i) ((i) & 0x1FF)

typedef struct {
    bool enabled;
    bool input;
    bool output;
    struct timespec start_time;
    uint32_t preset_ms; // задержка в миллисекундах
} TON_Timer, TOF_Timer, TP_Timer;

TON_Timer ton_timers[16];
TOF_Timer tof_timers[16];
TP_Timer tp_timers[16];

typedef struct {
    uint32_t value;
    uint32_t preset;
} CT_Counter;

CT_Counter ctu_counters[16];
CT_Counter ctd_counters[16];
CT_Counter ctud_counters[16];


//
// ──────────────────────────────────────────────────────────
//   ПРОТОТИПЫ ИНСТРУКЦИЙ
// ──────────────────────────────────────────────────────────
//

// арифметика
void op_add(uint32_t i);
void op_sub(uint32_t i);
void op_mul(uint32_t i);
void op_div(uint32_t i);
void op_mod(uint32_t i);
void op_expt(uint32_t i);
void op_abs(uint32_t i);
void op_sqrt(uint32_t i);
void op_ln(uint32_t i);
void op_log(uint32_t i);
void op_exp(uint32_t i);
void op_sin(uint32_t i);
void op_cos(uint32_t i);
void op_tan(uint32_t i);
void op_asin(uint32_t i);
void op_acos(uint32_t i);
void op_atan(uint32_t i);

// логика
void op_and(uint32_t i);
void op_or(uint32_t i);
void op_xor(uint32_t i);
void op_not(uint32_t i);

// сравнения
void op_eq(uint32_t i);
void op_ne(uint32_t i);
void op_gt(uint32_t i);
void op_ge(uint32_t i);
void op_lt(uint32_t i);
void op_le(uint32_t i);

// время / дата
void op_time(uint32_t i);
void op_date(uint32_t i);
void op_tod(uint32_t i);
void op_dt(uint32_t i);
void op_add_time(uint32_t i);
void op_sub_time(uint32_t i);
void op_year(uint32_t i);
void op_month(uint32_t i);
void op_day(uint32_t i);
void op_hour(uint32_t i);
void op_minute(uint32_t i);
void op_second(uint32_t i);

// строки
void op_len(uint32_t i);
void op_concat(uint32_t i);
void op_left(uint32_t i);
void op_right(uint32_t i);
void op_mid(uint32_t i);
void op_insert(uint32_t i);
void op_delete(uint32_t i);
void op_replace(uint32_t i);

// таймеры
void op_ton(uint32_t i);
void op_tof(uint32_t i);
void op_tp(uint32_t i);
void op_ctu(uint32_t i);
void op_ctd(uint32_t i);
void op_ctud(uint32_t i);

// прочее
void op_limit(uint32_t i);
void op_sel(uint32_t i);
void op_mux(uint32_t i);

#endif
