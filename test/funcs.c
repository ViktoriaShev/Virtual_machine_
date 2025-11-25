#include "funcs.h"
#include "vm32.h"
#include <string.h>
#include <time.h>

static inline uint32_t A(uint32_t i) { return RA(i); }
static inline uint32_t B(uint32_t i) { return reg[RB(i)]; }
static inline uint32_t C(uint32_t i) { return reg[RC(i)]; }
static inline void SetA(uint32_t i, uint32_t v) { reg[RA(i)] = v; }

// ──────────────────────────────
// арифметика
// ──────────────────────────────

void op_add(uint32_t i) { SetA(i, B(i) + C(i)); }
void op_sub(uint32_t i) { SetA(i, B(i) - C(i)); }
void op_mul(uint32_t i) { SetA(i, B(i) * C(i)); }
void op_div(uint32_t i) { uint32_t c=C(i); SetA(i, c? B(i)/c : 0); }
void op_mod(uint32_t i) { uint32_t c=C(i); SetA(i, c? B(i)%c : 0); }

void op_expt(uint32_t i) { SetA(i, pow(B(i), C(i))); }
void op_abs(uint32_t i)  { SetA(i, abs((int32_t)B(i))); }
void op_sqrt(uint32_t i) { SetA(i, sqrt(B(i))); }
void op_ln(uint32_t i)   { SetA(i, log(B(i))); }
void op_log(uint32_t i)  { SetA(i, log10(B(i))); }
void op_exp(uint32_t i)  { SetA(i, exp(B(i))); }
void op_sin(uint32_t i)  { SetA(i, sin(B(i))); }
void op_cos(uint32_t i)  { SetA(i, cos(B(i))); }
void op_tan(uint32_t i)  { SetA(i, tan(B(i))); }
void op_asin(uint32_t i) { SetA(i, asin(B(i))); }
void op_acos(uint32_t i) { SetA(i, acos(B(i))); }
void op_atan(uint32_t i) { SetA(i, atan(B(i))); }

// ──────────────────────────────
// логика
// ──────────────────────────────

void op_and(uint32_t i) { SetA(i, B(i) & C(i)); }
void op_or(uint32_t i)  { SetA(i, B(i) | C(i)); }
void op_xor(uint32_t i) { SetA(i, B(i) ^ C(i)); }
void op_not(uint32_t i) { SetA(i, ~B(i)); }

// ──────────────────────────────
// сравнения
// ──────────────────────────────

void op_eq(uint32_t i){ SetA(i, B(i) == C(i)); }
void op_ne(uint32_t i){ SetA(i, B(i) != C(i)); }
void op_gt(uint32_t i){ SetA(i, B(i) >  C(i)); }
void op_ge(uint32_t i){ SetA(i, B(i) >= C(i)); }
void op_lt(uint32_t i){ SetA(i, B(i) <  C(i)); }
void op_le(uint32_t i){ SetA(i, B(i) <= C(i)); }

// ──────────────────────────────
// время / дата (заглушки)
// ──────────────────────────────

void op_time(uint32_t i)       { SetA(i, 0); }
void op_date(uint32_t i)       { SetA(i, 0); }
void op_tod(uint32_t i)        { SetA(i, 0); }
void op_dt(uint32_t i)         { SetA(i, 0); }

void op_add_time(uint32_t i)   { SetA(i, B(i) + C(i)); }
void op_sub_time(uint32_t i)   { SetA(i, B(i) - C(i)); }

void op_year(uint32_t i)       { SetA(i, 0); }
void op_month(uint32_t i)      { SetA(i, 0); }
void op_day(uint32_t i)        { SetA(i, 0); }
void op_hour(uint32_t i)       { SetA(i, 0); }
void op_minute(uint32_t i)     { SetA(i, 0); }
void op_second(uint32_t i)     { SetA(i, 0); }

// ──────────────────────────────
// строки (минимальные)
// ──────────────────────────────


// Возвращает длину строки
void op_len(uint32_t i) {
    char *s = (char*)&mem[B(i)];
    SetA(i, strlen(s));
}

// Конкатенация строк
void op_concat(uint32_t i) {
    char *dest = (char*)&mem[B(i)];
    char *src  = (char*)&mem[C(i)];
    strncat(dest, src, MEM_SIZE * sizeof(uint32_t) - B(i) * sizeof(uint32_t) - strlen(dest) - 1);
}

// LEFT: первые C символов
void op_left(uint32_t i) {
    char *s = (char*)&mem[B(i)];
    uint32_t n = C(i);
    char buf[256];
    strncpy(buf, s, n);
    buf[n] = '\0';
    strcpy(s, buf);
}

// RIGHT: последние C символов
void op_right(uint32_t i) {
    char *s = (char*)&mem[B(i)];
    uint32_t n = C(i);
    size_t len = strlen(s);
    if (n > len) n = len;
    memmove(s, s + len - n, n);
    s[n] = '\0';
}

// MID: C символов начиная с B
void op_mid(uint32_t i) {
    char *s = (char*)&mem[B(i)];
    uint32_t start = B(i);
    uint32_t n = C(i);
    char buf[256];
    strncpy(buf, s + start, n);
    buf[n] = '\0';
    strcpy(s, buf);
}

// INSERT: вставить строку C в позицию B
void op_insert(uint32_t i) {
    char *s = (char*)&mem[RA(i)];
    char *ins = (char*)&mem[B(i)];
    uint32_t pos = C(i);
    size_t len_s = strlen(s);
    size_t len_ins = strlen(ins);
    if (pos > len_s) pos = len_s;
    memmove(s + pos + len_ins, s + pos, len_s - pos + 1);
    memcpy(s + pos, ins, len_ins);
}

// DELETE: удалить C символов с позиции B
void op_delete(uint32_t i) {
    char *s = (char*)&mem[RA(i)];
    uint32_t pos = B(i);
    uint32_t n = C(i);
    size_t len = strlen(s);
    if (pos > len) return;
    if (pos + n > len) n = len - pos;
    memmove(s + pos, s + pos + n, len - pos - n + 1);
}

// REPLACE: заменить C символов с позиции B на строку в RA
void op_replace(uint32_t i) {
    char *s = (char*)&mem[RA(i)];
    char *rep = (char*)&mem[B(i)];
    uint32_t pos = C(i);
    size_t len_s = strlen(s);
    size_t len_r = strlen(rep);
    if (pos > len_s) return;
    size_t end = pos + len_r;
    if (end < len_s) memmove(s + pos + len_r, s + end, len_s - end + 1);
    memcpy(s + pos, rep, len_r);
}

// ──────────────────────────────
// PLC таймеры (заглушки)
// ──────────────────────────────

void op_ton(uint32_t i) {
    uint32_t tid = RA(i);
    if (!ton_timers[tid].enabled) return;

    if (reg[RB(i)]) { // вход активен
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed = (now.tv_sec - ton_timers[tid].start_time.tv_sec) * 1000 +
                           (now.tv_nsec - ton_timers[tid].start_time.tv_nsec)/1000000;
        if (elapsed >= ton_timers[tid].preset_ms)
            ton_timers[tid].output = true;
    } else {
        ton_timers[tid].output = false;
        clock_gettime(CLOCK_MONOTONIC, &ton_timers[tid].start_time);
    }
    reg[RA(i)] = ton_timers[tid].output ? 1 : 0;
}

//TOF (таймер выключения)

void op_tof(uint32_t i) {
    uint32_t tid = RA(i);
    if (!tof_timers[tid].enabled) return;

    if (reg[RB(i)]) { // вход активен
        tof_timers[tid].output = true;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed = (now.tv_sec - tof_timers[tid].start_time.tv_sec) * 1000 +
                           (now.tv_nsec - tof_timers[tid].start_time.tv_nsec)/1000000;
        if (elapsed >= tof_timers[tid].preset_ms)
            tof_timers[tid].output = false;
    }
    reg[RA(i)] = tof_timers[tid].output ? 1 : 0;
}

//TP (импульс)

void op_tp(uint32_t i) {
    uint32_t tid = RA(i);
    if (!tp_timers[tid].enabled) return;

    if (reg[RB(i)]) { // фронт
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &tp_timers[tid].start_time);
        tp_timers[tid].output = true;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed = (now.tv_sec - tp_timers[tid].start_time.tv_sec) * 1000 +
                           (now.tv_nsec - tp_timers[tid].start_time.tv_nsec)/1000000;
        if (elapsed >= tp_timers[tid].preset_ms)
            tp_timers[tid].output = false;
    }
    reg[RA(i)] = tp_timers[tid].output ? 1 : 0;
}

//Счётчики

void op_ctu(uint32_t i) { ctu_counters[RA(i)].value += 1; reg[RA(i)] = ctu_counters[RA(i)].value; }
void op_ctd(uint32_t i) { if(ctd_counters[RA(i)].value>0) ctd_counters[RA(i)].value -= 1; reg[RA(i)] = ctd_counters[RA(i)].value; }
void op_ctud(uint32_t i) { 
    uint32_t idx = RA(i); 
    if (reg[RB(i)]) ctd_counters[idx].value--; 
    else ctu_counters[idx].value++;
    reg[idx] = ctu_counters[idx].value - ctd_counters[idx].value;
}

// ──────────────────────────────
// limit / sel / mux
// ──────────────────────────────

void op_limit(uint32_t i) {
    uint32_t x = reg[RA(i)];
    uint32_t lo = B(i);
    uint32_t hi = C(i);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    SetA(i, x);
}

void op_sel(uint32_t i) {
    uint32_t cond = B(i);
    uint32_t base = reg[RC(i)];
    uint32_t v1 = mr(base + 0);
    uint32_t v2 = mr(base + 1);
    SetA(i, cond ? v1 : v2);
}

void op_mux(uint32_t i) {
    uint32_t base = B(i);
    uint32_t idx  = C(i);
    SetA(i, mr(base + idx));
}
