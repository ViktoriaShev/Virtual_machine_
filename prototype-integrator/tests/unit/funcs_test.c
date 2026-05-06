#define _POSIX_C_SOURCE 200809L

#include "test_common.h"
#include "vm_lifecycle.h"
#include "funcs.h"

#include <time.h>
#include <stdatomic.h>

/* -------------------------------------------------------------
   Локальные pack-helpers.
   Opcode-биты тут не важны: op_* читают A/B/C/FIMM/IMM8.
   ------------------------------------------------------------- */
static inline uint32_t pack_rrr(uint32_t a, uint32_t b, uint32_t c) {
    return (((uint32_t)(a & 0xFFu)) << 17) |
           (((uint32_t)(b & 0xFFu)) << 9)  |
           ((uint32_t)(c & 0xFFu));
}

static inline uint32_t pack_rri(uint32_t a, uint32_t b, uint32_t imm8) {
    return (((uint32_t)(a & 0xFFu)) << 17) |
           (((uint32_t)(b & 0xFFu)) << 9)  |
           (1u << 8) |
           ((uint32_t)(uint8_t)imm8);
}

static void write_cstr(vm_state_t *vm, uint32_t addr, const char *s) {
    size_t n = strlen(s);
    memcpy(vm->mem + addr, s, n + 1);
}

static int cstr_eq_at(vm_state_t *vm, uint32_t addr, const char *expected) {
    return strcmp((char *)(vm->mem + addr), expected) == 0;
}

static void capture_tm(struct tm *out) {
    time_t t = time(NULL);
    localtime_r(&t, out);
}

/* ===== Арифметика ===== */

static int test_arithmetic_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 12;
    vm->reg[2] = 5;

    op_add(vm, pack_rrr(10, 1, 2));
    ASSERT_EQ_U32(17, vm->reg[10]);

    op_sub(vm, pack_rrr(11, 1, 2));
    ASSERT_EQ_U32(7, vm->reg[11]);

    op_mul(vm, pack_rrr(12, 1, 2));
    ASSERT_EQ_U32(60, vm->reg[12]);

    op_div(vm, pack_rrr(13, 1, 2));
    ASSERT_EQ_U32(2, vm->reg[13]);

    op_mod(vm, pack_rrr(14, 1, 2));
    ASSERT_EQ_U32(2, vm->reg[14]);

    vm->reg[1] = 2;
    vm->reg[2] = 10;
    op_expt(vm, pack_rrr(15, 1, 2));
    ASSERT_EQ_U32(1024, vm->reg[15]);

    vm_destroy(vm);
    return 0;
}

static int test_div_and_mod_by_zero(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 123;
    vm->reg[2] = 0;

    op_div(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[3]);

    op_mod(vm, pack_rrr(4, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[4]);

    vm_destroy(vm);
    return 0;
}

static int test_math_ops_exact_values(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = (uint32_t)(int32_t)-123;
    op_abs(vm, pack_rrr(2, 1, 0));
    ASSERT_EQ_U32(123, vm->reg[2]);

    vm->reg[1] = 9;
    op_sqrt(vm, pack_rrr(3, 1, 0));
    ASSERT_EQ_U32(3, vm->reg[3]);

    vm->reg[1] = 1;
    op_ln(vm, pack_rrr(4, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[4]);

    vm->reg[1] = 1;
    op_log(vm, pack_rrr(5, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[5]);

    vm->reg[1] = 0;
    op_exp(vm, pack_rrr(6, 1, 0));
    ASSERT_EQ_U32(1, vm->reg[6]);

    vm->reg[1] = 0;
    op_sin(vm, pack_rrr(7, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[7]);

    vm->reg[1] = 0;
    op_cos(vm, pack_rrr(8, 1, 0));
    ASSERT_EQ_U32(1, vm->reg[8]);

    vm->reg[1] = 0;
    op_tan(vm, pack_rrr(9, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[9]);

    vm->reg[1] = 0;
    op_asin(vm, pack_rrr(10, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[10]);

    vm->reg[1] = 1;
    op_acos(vm, pack_rrr(11, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[11]);

    vm->reg[1] = 0;
    op_atan(vm, pack_rrr(12, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[12]);

    vm_destroy(vm);
    return 0;
}

/* ===== Логика и сравнения ===== */

static int test_logic_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 0xF0F0F0F0u;
    vm->reg[2] = 0x0FF00FF0u;

    op_and(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(0x00F000F0u, vm->reg[3]);

    op_or(vm, pack_rrr(4, 1, 2));
    ASSERT_EQ_U32(0xFFF0FFF0u, vm->reg[4]);

    op_xor(vm, pack_rrr(5, 1, 2));
    ASSERT_EQ_U32(0xFF00FF00u, vm->reg[5]);

    vm->reg[1] = 0;
    op_not(vm, pack_rrr(6, 1, 0));
    ASSERT_EQ_U32(0xFFFFFFFFu, vm->reg[6]);

    vm_destroy(vm);
    return 0;
}

static int test_compare_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 10;
    vm->reg[2] = 20;

    op_eq(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[3]);

    op_ne(vm, pack_rrr(4, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[4]);

    op_gt(vm, pack_rrr(5, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[5]);

    op_ge(vm, pack_rrr(6, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[6]);

    op_lt(vm, pack_rrr(7, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[7]);

    op_le(vm, pack_rrr(8, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[8]);

    vm->reg[1] = 20;
    vm->reg[2] = 20;

    op_eq(vm, pack_rrr(9, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[9]);

    op_ge(vm, pack_rrr(10, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[10]);

    op_le(vm, pack_rrr(11, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[11]);

    vm_destroy(vm);
    return 0;
}

/* ===== Время и дата ===== */

static int test_time_date_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    struct tm tm_now;
    capture_tm(&tm_now);

    op_year(vm, pack_rrr(1, 0, 0));
    ASSERT_EQ_U32((uint32_t)(tm_now.tm_year + 1900), vm->reg[1]);

    op_month(vm, pack_rrr(2, 0, 0));
    ASSERT_EQ_U32((uint32_t)(tm_now.tm_mon + 1), vm->reg[2]);

    op_day(vm, pack_rrr(3, 0, 0));
    ASSERT_EQ_U32((uint32_t)tm_now.tm_mday, vm->reg[3]);

    op_hour(vm, pack_rrr(4, 0, 0));
    ASSERT_EQ_U32((uint32_t)tm_now.tm_hour, vm->reg[4]);

    op_minute(vm, pack_rrr(5, 0, 0));
    ASSERT_EQ_U32((uint32_t)tm_now.tm_min, vm->reg[5]);

    op_second(vm, pack_rrr(6, 0, 0));
    ASSERT_EQ_U32((uint32_t)tm_now.tm_sec, vm->reg[6]);

    op_time(vm, pack_rrr(7, 0, 0));
    ASSERT_EQ_U32((uint32_t)(tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec), vm->reg[7]);

    op_date(vm, pack_rrr(8, 0, 0));
    ASSERT_EQ_U32((uint32_t)((tm_now.tm_year + 1900) * 10000 + (tm_now.tm_mon + 1) * 100 + tm_now.tm_mday), vm->reg[8]);

    op_tod(vm, pack_rrr(9, 0, 0));
    ASSERT_EQ_U32((uint32_t)(tm_now.tm_hour * 10000 + tm_now.tm_min * 100 + tm_now.tm_sec), vm->reg[9]);

    /* Проверяем текущую реализацию op_dt как она написана сейчас */
    op_dt(vm, pack_rrr(10, 0, 0));
    ASSERT_EQ_U32((uint32_t)((tm_now.tm_year + 1900) * 1000000 +
                             (tm_now.tm_mon + 1) * 10000 +
                             tm_now.tm_mday * 100 +
                             tm_now.tm_hour),
                  vm->reg[10]);

    vm->reg[1] = 1000;
    vm->reg[2] = 250;

    op_add_time(vm, pack_rrr(11, 1, 2));
    ASSERT_EQ_U32(1250, vm->reg[11]);

    op_sub_time(vm, pack_rrr(12, 1, 2));
    ASSERT_EQ_U32(750, vm->reg[12]);

    vm_destroy(vm);
    return 0;
}

/* ===== Строки ===== */

static int test_string_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    write_cstr(vm, 0x0100, "hello");
    write_cstr(vm, 0x0120, "world");

    vm->reg[1] = 0x0100;
    op_len(vm, pack_rrr(2, 1, 0));
    ASSERT_EQ_U32(5, vm->reg[2]);

    vm->reg[1] = 0x0100;
    vm->reg[2] = 0x0120;
    op_concat(vm, pack_rrr(0, 1, 2));
    ASSERT_TRUE(cstr_eq_at(vm, 0x0100, "helloworld"));

    write_cstr(vm, 0x0200, "abcdef");
    vm->reg[1] = 0x0200;
    op_left(vm, pack_rri(0, 1, 3));
    ASSERT_TRUE(cstr_eq_at(vm, 0x0200, "abc"));

    write_cstr(vm, 0x0210, "abcdef");
    vm->reg[1] = 0x0210;
    op_right(vm, pack_rri(0, 1, 3));
    ASSERT_TRUE(cstr_eq_at(vm, 0x0210, "def"));

    write_cstr(vm, 0x0220, "abcdef");
    vm->reg[1] = 0x0220;
    op_mid(vm, pack_rri(0, 1, 2));
    ASSERT_TRUE(cstr_eq_at(vm, 0x0220, "cdef"));

    write_cstr(vm, 0x0300, "abcd");
    write_cstr(vm, 0x0310, "XY");
    vm->reg[1] = 0x0300;   /* dest address */
    vm->reg[2] = 0x0310;   /* insert string address */
    op_insert(vm, pack_rri(1, 2, 2));
    ASSERT_TRUE(cstr_eq_at(vm, 0x0300, "abXYcd"));

    write_cstr(vm, 0x0320, "abcd");
    vm->reg[1] = 0x0320; /* addr */
    vm->reg[2] = 1;      /* pos */
    op_delete(vm, pack_rri(1, 2, 1)); /* A=1, B=2, imm=1 */
    ASSERT_TRUE(cstr_eq_at(vm, 0x0320, "acd"));

    write_cstr(vm, 0x0330, "abcdef");
    write_cstr(vm, 0x0340, "Z");
    vm->reg[1] = 0x0330;
    vm->reg[2] = 0x0340;
    op_replace(vm, pack_rri(1, 2, 2)); /* replace from pos 2 with "Z" */
    ASSERT_TRUE(cstr_eq_at(vm, 0x0330, "abZdef"));

    vm_destroy(vm);
    return 0;
}

static int test_string_bounds_and_noop_paths(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    /* len out of bounds -> 0 */
    vm->reg[1] = VM_MEM_BYTES;
    op_len(vm, pack_rrr(2, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[2]);

    /* concat with invalid addresses must not touch memory */
    vm->mem[VM_MEM_BYTES - 1] = 0xAA;
    vm->mem[VM_MEM_BYTES - 2] = 0xBB;
    vm->reg[1] = VM_MEM_BYTES;
    vm->reg[2] = VM_MEM_BYTES;
    op_concat(vm, pack_rrr(0, 1, 2));
    ASSERT_EQ_U32(0xAA, vm->mem[VM_MEM_BYTES - 1]);
    ASSERT_EQ_U32(0xBB, vm->mem[VM_MEM_BYTES - 2]);

    /* right with n >= len is a no-op */
    write_cstr(vm, 0x0100, "abc");
    vm->reg[1] = 0x0100;
    op_right(vm, pack_rri(0, 1, 3));
    ASSERT_TRUE(cstr_eq_at(vm, 0x0100, "abc"));

    vm_destroy(vm);
    return 0;
}

/* ===== Таймеры ===== */

static int test_ton_tof_tp_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    /* TON: id=7, preset stored in reg[7] because op_ton reads RC(i) */
    vm->reg[7] = 100;
    vm->reg[1] = 1;   /* input */
    vm->time_ms = 1000;

    op_ton(vm, pack_rri(2, 1, 7));
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->time_ms = 1099;
    vm->reg[2] = 0;
    op_ton(vm, pack_rri(2, 1, 7));
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->time_ms = 1100;
    vm->reg[2] = 0;
    op_ton(vm, pack_rri(2, 1, 7));
    ASSERT_EQ_U32(1, vm->reg[2]);

    /* TOF: common PLC behavior */
    vm->reg[8] = 50;
    vm->time_ms = 2000;
    vm->reg[1] = 1;
    op_tof(vm, pack_rri(3, 1, 8));
    ASSERT_EQ_U32(1, vm->reg[3]);

    vm->time_ms = 2010;
    vm->reg[1] = 0;
    vm->reg[3] = 0;
    op_tof(vm, pack_rri(3, 1, 8));
    ASSERT_EQ_U32(1, vm->reg[3]);

    vm->time_ms = 2060;
    vm->reg[3] = 0;
    op_tof(vm, pack_rri(3, 1, 8));
    ASSERT_EQ_U32(0, vm->reg[3]);

    /* TP: pulse */
    vm->reg[9] = 50;
    vm->time_ms = 3000;
    vm->reg[1] = 1;
    op_tp(vm, pack_rri(4, 1, 9));
    ASSERT_EQ_U32(1, vm->reg[4]);

    vm->time_ms = 3020;
    vm->reg[4] = 0;
    op_tp(vm, pack_rri(4, 1, 9));
    ASSERT_EQ_U32(1, vm->reg[4]);

    vm->time_ms = 3051;
    vm->reg[4] = 0;
    op_tp(vm, pack_rri(4, 1, 9));
    ASSERT_EQ_U32(0, vm->reg[4]);

    vm_destroy(vm);
    return 0;
}

static int test_timer_invalid_ids_are_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 1;
    vm->reg[7] = 100;

    /* id = 255 -> invalid, should return without touching outputs */
    vm->reg[2] = 1234;
    op_ton(vm, pack_rri(2, 1, 255));
    ASSERT_EQ_U32(1234, vm->reg[2]);

    vm->reg[3] = 5678;
    op_tof(vm, pack_rri(3, 1, 255));
    ASSERT_EQ_U32(5678, vm->reg[3]);

    vm->reg[4] = 9999;
    op_tp(vm, pack_rri(4, 1, 255));
    ASSERT_EQ_U32(9999, vm->reg[4]);

    vm_destroy(vm);
    return 0;
}

/* ===== Счётчики ===== */

static int test_ctu_ctd_ctud_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    /* CTU */
    vm->reg[2] = 0;      /* id */
    vm->reg[3] = 0;      /* input */
    op_ctu(vm, pack_rri(2, 3, 2));   /* preset = 2 */
    ASSERT_EQ_U32(0, vm->ctu_counters[0].value);
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->reg[2] = 0;
    vm->reg[3] = 1;      /* rising */
    op_ctu(vm, pack_rri(2, 3, 2));
    ASSERT_EQ_U32(1, vm->ctu_counters[0].value);
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->reg[2] = 0;
    vm->reg[3] = 0;
    op_ctu(vm, pack_rri(2, 3, 2));
    ASSERT_EQ_U32(1, vm->ctu_counters[0].value);

    vm->reg[2] = 0;
    vm->reg[3] = 1;      /* second rise */
    op_ctu(vm, pack_rri(2, 3, 2));
    ASSERT_EQ_U32(2, vm->ctu_counters[0].value);
    ASSERT_EQ_U32(1, vm->reg[2]);

    /* CTD */
    vm->ctd_counters[1].value = 3;
    vm->reg[2] = 1;      /* id */
    vm->reg[3] = 0;
    op_ctd(vm, pack_rri(2, 3, 1));   /* preset = 1 */
    ASSERT_EQ_U32(3, vm->ctd_counters[1].value);

    vm->reg[2] = 1;
    vm->reg[3] = 1;      /* rising => decrement */
    op_ctd(vm, pack_rri(2, 3, 1));
    ASSERT_EQ_U32(2, vm->ctd_counters[1].value);
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->reg[2] = 1;
    vm->reg[3] = 0;
    op_ctd(vm, pack_rri(2, 3, 1));

    vm->reg[2] = 1;
    vm->reg[3] = 1;
    op_ctd(vm, pack_rri(2, 3, 1));
    ASSERT_EQ_U32(1, vm->ctd_counters[1].value);
    ASSERT_EQ_U32(1, vm->reg[2]);

    /* CTUD */
    vm->ctud_counters[2].value = 1;
    vm->reg[2] = 2;      /* id */
    vm->reg[3] = 1;      /* up */
    vm->reg[4] = 0;      /* down */
    op_ctud(vm, pack_rrr(2, 3, 4));
    ASSERT_EQ_U32(2, vm->ctud_counters[2].value);
    ASSERT_EQ_U32(2, vm->reg[2]);

    vm->reg[2] = 2;
    vm->reg[3] = 0;
    vm->reg[4] = 1;
    op_ctud(vm, pack_rrr(2, 3, 4));
    ASSERT_EQ_U32(1, vm->ctud_counters[2].value);
    ASSERT_EQ_U32(1, vm->reg[2]);

    vm_destroy(vm);
    return 0;
}

static int test_counter_invalid_ids_are_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[2] = (uint32_t)(MAX_TIMERS + 1);
    vm->reg[3] = 1;
    op_ctu(vm, pack_rri(2, 3, 1));
    ASSERT_EQ_U32((uint32_t)(MAX_TIMERS + 1), vm->reg[2]);

    vm->reg[2] = (uint32_t)(MAX_TIMERS + 1);
    vm->reg[3] = 1;
    op_ctd(vm, pack_rri(2, 3, 1));
    ASSERT_EQ_U32((uint32_t)(MAX_TIMERS + 1), vm->reg[2]);

    vm->reg[2] = (uint32_t)(MAX_TIMERS + 1);
    vm->reg[3] = 1;
    vm->reg[4] = 0;
    op_ctud(vm, pack_rrr(2, 3, 4));
    ASSERT_EQ_U32((uint32_t)(MAX_TIMERS + 1), vm->reg[2]);

    vm_destroy(vm);
    return 0;
}

/* ===== limit / sel / mux ===== */

static int test_limit_sel_mux_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[3] = 100;
    vm->reg[1] = 10;
    vm->reg[2] = 50;
    op_limit(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(50, vm->reg[3]);

    vm->reg[3] = 5;
    op_limit(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(10, vm->reg[3]);

    vm->reg[3] = 20;
    op_limit(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(20, vm->reg[3]);

    vm_mw32(vm, 0x0400, 111);
    vm_mw32(vm, 0x0404, 222);

    vm->reg[1] = 1;      /* cond */
    vm->reg[2] = 0x0400; /* base */
    op_sel(vm, pack_rrr(4, 1, 2));
    ASSERT_EQ_U32(111, vm->reg[4]);

    vm->reg[1] = 0;
    op_sel(vm, pack_rrr(5, 1, 2));
    ASSERT_EQ_U32(222, vm->reg[5]);

    vm_mw32(vm, 0x0500, 10);
    vm_mw32(vm, 0x0504, 20);
    vm_mw32(vm, 0x0508, 30);

    vm->reg[1] = 0x0500;
    op_mux(vm, pack_rri(6, 1, 2));
    ASSERT_EQ_U32(30, vm->reg[6]);

    vm->reg[1] = VM_MEM_BYTES - 4;
    op_mux(vm, pack_rri(7, 1, 1000));
    ASSERT_EQ_U32(0, vm->reg[7]);

    vm_destroy(vm);
    return 0;
}

static int test_sel_mux_bounds_are_safe(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 1;
    vm->reg[2] = VM_MEM_BYTES; /* invalid base */
    vm->reg[3] = 777;
    op_sel(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[3]);

    vm->reg[1] = VM_MEM_BYTES;
    vm->reg[4] = 888;
    op_mux(vm, pack_rri(4, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[4]);

    vm_destroy(vm);
    return 0;
}

/* ===== Edge detectors / latches / demux ===== */

static int test_edge_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[1] = 0;
    op_rising_edge(vm, pack_rri(2, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->reg[1] = 1;
    op_rising_edge(vm, pack_rri(2, 1, 0));
    ASSERT_EQ_U32(1, vm->reg[2]);

    vm->reg[1] = 1;
    op_rising_edge(vm, pack_rri(2, 1, 0));
    ASSERT_EQ_U32(0, vm->reg[2]);

    vm->reg[1] = 1;
    op_falling_edge(vm, pack_rri(3, 1, 1));
    ASSERT_EQ_U32(0, vm->reg[3]);

    vm->reg[1] = 0;
    op_falling_edge(vm, pack_rri(3, 1, 1));
    ASSERT_EQ_U32(1, vm->reg[3]);

    vm->reg[1] = 0;
    op_edge_both(vm, pack_rri(4, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[4]);

    vm->reg[1] = 1;
    op_edge_both(vm, pack_rri(4, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[4]);

    vm->reg[1] = 0;
    op_edge_both(vm, pack_rri(4, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[4]);

    vm_destroy(vm);
    return 0;
}

static int test_latch_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    /* RS latch: R has priority */
    vm->reg[1] = 1; /* S */
    vm->reg[2] = 0; /* R */
    op_rs_latch(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[3]);

    vm->reg[1] = 0;
    vm->reg[2] = 0;
    op_rs_latch(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[3]);

    vm->reg[1] = 0;
    vm->reg[2] = 1;
    op_rs_latch(vm, pack_rrr(3, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[3]);

    /* SR latch: S has priority */
    vm->reg[1] = 1;
    vm->reg[2] = 1;
    op_sr_latch(vm, pack_rrr(4, 1, 2));
    ASSERT_EQ_U32(1, vm->reg[4]);

    vm->reg[1] = 0;
    vm->reg[2] = 1;
    op_sr_latch(vm, pack_rrr(4, 1, 2));
    ASSERT_EQ_U32(0, vm->reg[4]);

    vm_destroy(vm);
    return 0;
}

static int test_demux_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->reg[3] = 0xAABBCCDDu;
    vm->reg[1] = 0x0600; /* base */
    op_demux(vm, pack_rri(3, 1, 2));
    ASSERT_EQ_U32(0xAABBCCDDu, vm_mr32(vm, 0x0608));

    uint8_t before = vm->mem[VM_MEM_BYTES - 1];
    vm->reg[3] = 0x11223344u;
    vm->reg[1] = VM_MEM_BYTES - 2;
    op_demux(vm, pack_rri(3, 1, 1)); /* out of bounds */
    ASSERT_EQ_U32(before, vm->mem[VM_MEM_BYTES - 1]);

    vm_destroy(vm);
    return 0;
}

/* ===== JMP / EXIT / HALT / NOP ===== */

static int test_control_ops(void) {
    vm_state_t *vm = vm_create();
    ASSERT_NOT_NULL(vm);

    vm->PC = 0x3000;
    vm->reg[1] = 0x3456;
    op_jmp(vm, pack_rrr(0, 1, 0));
    ASSERT_EQ_U32(0x3456, vm->PC);

    vm->PC = 0x4000;
    vm->reg[1] = 0x5000;
    vm->reg[2] = 1;
    op_jmp_if(vm, pack_rrr(0, 1, 2));
    ASSERT_EQ_U32(0x5000, vm->PC);

    vm->PC = 0x6000;
    vm->reg[1] = 0x7000;
    vm->reg[2] = 0;
    op_jmp_if(vm, pack_rrr(0, 1, 2));
    ASSERT_EQ_U32(0x6000, vm->PC);

    vm->PC = 0x8000;
    vm->reg[1] = 0x9000;
    vm->reg[2] = 0;
    op_jmp_if_not(vm, pack_rrr(0, 1, 2));
    ASSERT_EQ_U32(0x9000, vm->PC);

    vm->PC = 0xA000;
    vm->reg[1] = 0xB000;
    vm->reg[2] = 1;
    op_jmp_if_not(vm, pack_rrr(0, 1, 2));
    ASSERT_EQ_U32(0xA000, vm->PC);

    vm->running = true;
    atomic_store(&vm->stop_requested, false);
    op_halt(vm, pack_rrr(0, 0, 0));
    ASSERT_FALSE(vm->running);
    ASSERT_TRUE(atomic_load(&vm->stop_requested));

    vm->running = true;
    atomic_store(&vm->stop_requested, false);
    vm->exit_code = 0;
    op_exit(vm, pack_rri(0, 0, 42));
    ASSERT_FALSE(vm->running);
    ASSERT_TRUE(atomic_load(&vm->stop_requested));
    ASSERT_EQ_U32(42, vm->exit_code);

    uint32_t pc_before = 0x1234;
    vm->PC = pc_before;
    vm->reg[1] = 77;
    op_nop(vm, pack_rrr(0, 0, 0));
    ASSERT_EQ_U32(pc_before, vm->PC);
    ASSERT_EQ_U32(77, vm->reg[1]);

    vm_destroy(vm);
    return 0;
}

int main(void) {
    ASSERT_EQ_U32(0, test_arithmetic_ops());
    ASSERT_EQ_U32(0, test_div_and_mod_by_zero());
    ASSERT_EQ_U32(0, test_math_ops_exact_values());
    ASSERT_EQ_U32(0, test_logic_ops());
    ASSERT_EQ_U32(0, test_compare_ops());
    ASSERT_EQ_U32(0, test_time_date_ops());
    ASSERT_EQ_U32(0, test_string_ops());
    ASSERT_EQ_U32(0, test_string_bounds_and_noop_paths());
    ASSERT_EQ_U32(0, test_ton_tof_tp_ops());
    ASSERT_EQ_U32(0, test_timer_invalid_ids_are_safe());
    ASSERT_EQ_U32(0, test_ctu_ctd_ctud_ops());
    ASSERT_EQ_U32(0, test_counter_invalid_ids_are_safe());
    ASSERT_EQ_U32(0, test_limit_sel_mux_ops());
    ASSERT_EQ_U32(0, test_sel_mux_bounds_are_safe());
    ASSERT_EQ_U32(0, test_edge_ops());
    ASSERT_EQ_U32(0, test_latch_ops());
    ASSERT_EQ_U32(0, test_demux_ops());
    ASSERT_EQ_U32(0, test_control_ops());

    printf("[PASS] unit/funcs_unit_test\n");
    return 0;
}