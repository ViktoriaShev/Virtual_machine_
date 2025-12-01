
#ifndef VM_H
#define VM_H

#include "vm32.h"

#include <time.h>
#include <stdint.h>
#include <unistd.h>
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

typedef struct {
    bool enabled;
    bool input;
    bool output;
    struct timespec start_time; /* требует <time.h> в .c */
    uint32_t preset_ms;
} TON_Timer;

typedef struct {
    bool enabled;
    bool input;
    bool output;
    struct timespec start_time;
    uint32_t preset_ms;
} TOF_Timer;

typedef struct {
    bool enabled;
    bool input;
    bool output;
    bool pulse_generated;
    struct timespec start_time;
    uint32_t preset_ms;
} TP_Timer;

/* счётчики */
typedef struct {
    uint32_t value;
    uint32_t preset;
} CT_Counter;


/* маски/шаблоны декодирования (формат инструкции):
   [ opcode:7 ][ A:8 ][ B:8 ][ C:9 ]  (всего 32 бита)
*/
#define OPC(i)  (uint8_t)(((i) >> 25) & 0x7F)
#define RA(i)   (uint8_t)(((i) >> 17) & 0xFF)
#define RB(i)   (uint8_t)(((i) >> 9) & 0xFF)
#define RC(i)   (uint32_t)((i) & 0x1FFU)

/* --- Удобные обёртки для operand helpers (A/B/C как в исходнике) --- */
 inline  uint32_t A(uint32_t i) { return RA(i); }       // номер регистра A
 inline  uint32_t Bv(uint32_t i) { return reg[RB(i)]; } // значение B (обычно адрес)
 inline  uint32_t Cv(uint32_t i) { return reg[RC(i)]; } // значение C (число или адрес)
 inline void SetA_val(uint32_t i, uint32_t v) { reg[RA(i)] = v; }


// Типизированные значения для VM (PLC-like)
typedef struct {
    int32_t hours;   // 0..23
    int32_t minutes; // 0..59
    int32_t seconds; // 0..59
} TOD_t; // Time of Day

typedef struct {
    int32_t year;    // полный год, например 2025
    int32_t month;   // 1..12
    int32_t day;     // 1..31
} DATE_t;

typedef struct {
    DATE_t date;
    TOD_t  time;
} DT_t; // DateTime

extern CT_Counter ctu_counters[16];
extern CT_Counter ctd_counters[16];
extern CT_Counter ctud_counters[16];

/* ----------------------------
   Прототипы инструкций (определены в funcs.c)
   ---------------------------- */
/* арифметика */
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

/* логика */
void op_and(uint32_t i);
void op_or(uint32_t i);
void op_xor(uint32_t i);
void op_not(uint32_t i);

/* сравнения */
void op_eq(uint32_t i);
void op_ne(uint32_t i);
void op_gt(uint32_t i);
void op_ge(uint32_t i);
void op_lt(uint32_t i);
void op_le(uint32_t i);

/* время / дата */
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

/* строки */
void op_len(uint32_t i);
void op_concat(uint32_t i);
void op_left(uint32_t i);
void op_right(uint32_t i);
void op_mid(uint32_t i);
void op_insert(uint32_t i);
void op_delete(uint32_t i);
void op_replace(uint32_t i);

/* таймеры / счётчики */
void op_ton(uint32_t i);
void op_tof(uint32_t i);
void op_tp(uint32_t i);
void op_ctu(uint32_t i);
void op_ctd(uint32_t i);
void op_ctud(uint32_t i);

/* прочее */
void op_limit(uint32_t i);
void op_sel(uint32_t i);
void op_mux(uint32_t i);

void op_jmp(uint32_t i);
void op_jmp_if(uint32_t i);
void op_jmp_if_not(uint32_t i);

void op_halt(uint32_t i);  // НОВАЯ!
void op_nop(uint32_t i);   // NOP для заполнения
#endif /* VM32_H */