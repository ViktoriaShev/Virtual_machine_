#ifndef FUNCS_H
#define FUNCS_H

#include "main.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* Forward declaration */
typedef struct vm_state vm_state_t;

/* Типизированные значения для VM (PLC-like) */
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


/* Формат инструкции: [ opcode:7 ][ A:8 ][ B:8 ][ IMM:1 ][ C:8 ] */
#define OPC(i)  (uint8_t)(((i) >> 25) & 0x7F)
#define RA(i)   (uint8_t)(((i) >> 17) & 0xFF)
#define RB(i)   (uint8_t)(((i) >> 9) & 0xFF)
#define RC(i)   (uint32_t)((i) & 0xFF)           // Теперь только 8 бит!

/* Флаг immediate (бит 8) */
#define FIMM(i) (((i) >> 8) & 1)

/* 8-битный immediate с знаковым расширением */
#define IMM8(i)     ((i) & 0xFF)

/* Функция знакового расширения */
static inline uint32_t sext(uint32_t n, int b) {
    return ((n >> (b - 1)) & 1) ? (n | (0xFFFFFFFF << b)) : n;
}

#define SEXTIMM8(i) sext((i) & 0xFF, 8)

/* Макросы для доступа к регистрам VM через vm_state_t */
#define Av(vm, i) ((vm)->reg[RA(i)])
#define A(i) (RA(i))
#define Bv(vm, i) ((vm)->reg[RB(i)])
#define Cv(vm, i) ((vm)->reg[RC(i)])

/* Получение значения C: либо регистр (0-255), либо immediate (-128..+127) */
#define Cv_or_imm(vm, i) (FIMM(i) ? SEXTIMM8(i) : (vm)->reg[RC(i)])

/* Установка значения регистра A */
#define SetA_val(vm, i, v) ((vm)->reg[RA(i)] = (v))

#ifdef UNIT_TEST
void vm_counters_reset(vm_state_t *vm);
#endif

/* --- Прототипы инструкций (определены в funcs.c) --- */

/* Арифметика */
void op_add(vm_state_t *vm, uint32_t i);
void op_sub(vm_state_t *vm, uint32_t i);
void op_mul(vm_state_t *vm, uint32_t i);
void op_div(vm_state_t *vm, uint32_t i);
void op_mod(vm_state_t *vm, uint32_t i);
void op_expt(vm_state_t *vm, uint32_t i);
void op_abs(vm_state_t *vm, uint32_t i);
void op_sqrt(vm_state_t *vm, uint32_t i);
void op_ln(vm_state_t *vm, uint32_t i);
void op_log(vm_state_t *vm, uint32_t i);
void op_exp(vm_state_t *vm, uint32_t i);
void op_sin(vm_state_t *vm, uint32_t i);
void op_cos(vm_state_t *vm, uint32_t i);
void op_tan(vm_state_t *vm, uint32_t i);
void op_asin(vm_state_t *vm, uint32_t i);
void op_acos(vm_state_t *vm, uint32_t i);
void op_atan(vm_state_t *vm, uint32_t i);

/* Логика */
void op_and(vm_state_t *vm, uint32_t i);
void op_or(vm_state_t *vm, uint32_t i);
void op_xor(vm_state_t *vm, uint32_t i);
void op_not(vm_state_t *vm, uint32_t i);

/* Сравнения */
void op_eq(vm_state_t *vm, uint32_t i);
void op_ne(vm_state_t *vm, uint32_t i);
void op_gt(vm_state_t *vm, uint32_t i);
void op_ge(vm_state_t *vm, uint32_t i);
void op_lt(vm_state_t *vm, uint32_t i);
void op_le(vm_state_t *vm, uint32_t i);

/* Время / дата */
void op_time(vm_state_t *vm, uint32_t i);
void op_date(vm_state_t *vm, uint32_t i);
void op_tod(vm_state_t *vm, uint32_t i);
void op_dt(vm_state_t *vm, uint32_t i);
void op_add_time(vm_state_t *vm, uint32_t i);
void op_sub_time(vm_state_t *vm, uint32_t i);
void op_year(vm_state_t *vm, uint32_t i);
void op_month(vm_state_t *vm, uint32_t i);
void op_day(vm_state_t *vm, uint32_t i);
void op_hour(vm_state_t *vm, uint32_t i);
void op_minute(vm_state_t *vm, uint32_t i);
void op_second(vm_state_t *vm, uint32_t i);

/* Строки */
void op_len(vm_state_t *vm, uint32_t i);
void op_concat(vm_state_t *vm, uint32_t i);
void op_left(vm_state_t *vm, uint32_t i);
void op_right(vm_state_t *vm, uint32_t i);
void op_mid(vm_state_t *vm, uint32_t i);
void op_insert(vm_state_t *vm, uint32_t i);
void op_delete(vm_state_t *vm, uint32_t i);
void op_replace(vm_state_t *vm, uint32_t i);

/* Таймеры / счётчики */
void op_ton(vm_state_t *vm, uint32_t i);
void op_tof(vm_state_t *vm, uint32_t i);
void op_tp(vm_state_t *vm, uint32_t i);
void op_ctu(vm_state_t *vm, uint32_t i);
void op_ctd(vm_state_t *vm, uint32_t i);
void op_ctud(vm_state_t *vm, uint32_t i);

/* Прочее */
void op_limit(vm_state_t *vm, uint32_t i);
void op_sel(vm_state_t *vm, uint32_t i);
void op_mux(vm_state_t *vm, uint32_t i);

/* IEC/SCADA: edge detectors, latches, demux */
void op_rising_edge(vm_state_t *vm, uint32_t i);
void op_falling_edge(vm_state_t *vm, uint32_t i);
void op_edge_both(vm_state_t *vm, uint32_t i);

void op_rs_latch(vm_state_t *vm, uint32_t i); /* R has priority */
void op_sr_latch(vm_state_t *vm, uint32_t i); /* S has priority */

void op_demux(vm_state_t *vm, uint32_t i);

void op_jmp(vm_state_t *vm, uint32_t i);
void op_jmp_if(vm_state_t *vm, uint32_t i);
void op_jmp_if_not(vm_state_t *vm, uint32_t i);

void op_exit(vm_state_t *vm, uint32_t i);
void op_halt(vm_state_t *vm, uint32_t i);
void op_nop(vm_state_t *vm, uint32_t i);

#endif /* FUNCS_H */