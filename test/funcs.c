#define _POSIX_C_SOURCE 200809L
#include "vm32.h"
#include "funcs.h"
#include "timers.h"
#include <string.h>
#include <stdio.h>
#include <strings.h> 
#include <math.h>

/* --- Вспомогательные функции безопасности для работы со строками/памятью --- */

static inline size_t safe_mem_remaining(vm_state_t *vm, uint32_t addr) {
    if (addr >= VM_MEM_BYTES) return 0;
    return (size_t)(VM_MEM_BYTES - addr);
}

/* strlen, но ограниченная рамками памяти VM */
static size_t safe_strlen_at(vm_state_t *vm, uint32_t addr) {
    size_t rem = safe_mem_remaining(vm, addr);
    size_t i = 0;
    if (rem == 0) return 0;
    while (i < rem && vm->mem[addr + i] != '\0') i++;
    return i;
}

/* копирует n байт из srcAddr в destAddr, с учётом границ VM */
static size_t safe_memcpy_from_to(vm_state_t *vm, uint32_t destAddr, const uint8_t *src, size_t n) {
    size_t rem = safe_mem_remaining(vm, destAddr);
    size_t to_copy = n;
    if (to_copy > rem) to_copy = rem;
    if (to_copy > 0) memcpy(vm->mem + destAddr, src, to_copy);
    return to_copy;
}

/* memmove внутри mem (байтовая) с учетом границ */
static void safe_memmove(vm_state_t *vm, uint32_t destAddr, uint32_t srcAddr, size_t n) {
    size_t rem_dest = safe_mem_remaining(vm, destAddr);
    size_t rem_src = safe_mem_remaining(vm, srcAddr);
    size_t max_copy = rem_dest < rem_src ? rem_dest : rem_src;
    if (n > max_copy) n = max_copy;
    memmove(vm->mem + destAddr, vm->mem + srcAddr, n);
}

/* записать нультерминатор, если адрес валиден */
static void safe_set_null(vm_state_t *vm, uint32_t addr) {
    if (addr < VM_MEM_BYTES) vm->mem[addr] = 0;
}

/* Возвращает локальное время */
static struct tm current_tm(void) {
    time_t t = time(NULL);
    struct tm tm_now;
    localtime_r(&t, &tm_now);
    return tm_now;
}

/* Конверсия struct tm -> TOD_t */
static TOD_t tm_to_tod(struct tm t) {
    TOD_t tod;
    tod.hours = t.tm_hour;
    tod.minutes = t.tm_min;
    tod.seconds = t.tm_sec;
    return tod;
}

/* Конверсия struct tm -> DATE_t */
static DATE_t tm_to_date(struct tm t) {
    DATE_t date;
    date.year = t.tm_year + 1900;
    date.month = t.tm_mon + 1;
    date.day = t.tm_mday;
    return date;
}

/* Конверсия struct tm -> DT_t */
static DT_t tm_to_dt(struct tm t) {
    DT_t dt;
    dt.date = tm_to_date(t);
    dt.time = tm_to_tod(t);
    return dt;
}

/* Преобразование TOD_t в секунды с начала дня */
static int32_t tod_to_seconds(TOD_t t) {
    return t.hours*3600 + t.minutes*60 + t.seconds;
}

/* ===== Арифметика ===== */

void op_add(vm_state_t *vm, uint32_t i) { 
    SetA_val(vm, i, Bv(vm, i) + Cv_or_imm(vm, i)); 
}

void op_sub(vm_state_t *vm, uint32_t i) { 
    SetA_val(vm, i, Bv(vm, i) - Cv_or_imm(vm, i)); 
}

void op_mul(vm_state_t *vm, uint32_t i) { 
    SetA_val(vm, i, Bv(vm, i) * Cv_or_imm(vm, i)); 
}

void op_div(vm_state_t *vm, uint32_t i) { 
    uint32_t c = Cv_or_imm(vm, i); 
    SetA_val(vm, i, c ? Bv(vm, i)/c : 0); 
}

void op_mod(vm_state_t *vm, uint32_t i) { 
    uint32_t c = Cv_or_imm(vm, i); 
    SetA_val(vm, i, c ? Bv(vm, i)%c : 0); 
}

void op_expt(vm_state_t *vm, uint32_t i) { 
    SetA_val(vm, i, (uint32_t)pow((double)Bv(vm, i), (double)Cv_or_imm(vm, i))); 
}

void op_abs(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, (uint32_t)abs((int32_t)Bv(vm, i))); }
void op_sqrt(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, (uint32_t)sqrt((double)Bv(vm, i))); }
void op_ln(vm_state_t *vm, uint32_t i)   { SetA_val(vm, i, (uint32_t)log((double)Bv(vm, i))); }
void op_log(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, (uint32_t)log10((double)Bv(vm, i))); }
void op_exp(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, (uint32_t)exp((double)Bv(vm, i))); }
void op_sin(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, (uint32_t)sin((double)Bv(vm, i))); }
void op_cos(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, (uint32_t)cos((double)Bv(vm, i))); }
void op_tan(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, (uint32_t)tan((double)Bv(vm, i))); }
void op_asin(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, (uint32_t)asin((double)Bv(vm, i))); }
void op_acos(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, (uint32_t)acos((double)Bv(vm, i))); }
void op_atan(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, (uint32_t)atan((double)Bv(vm, i))); }

/* ===== Логика ===== */

void op_and(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) & Cv_or_imm(vm, i)); }
void op_or(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, Bv(vm, i) | Cv_or_imm(vm, i)); }
void op_xor(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) ^ Cv_or_imm(vm, i)); }
void op_not(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, ~Bv(vm, i)); }

/* ===== Сравнения ===== */

void op_eq(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) == Cv_or_imm(vm, i) ? 1 : 0); }
void op_ne(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) != Cv_or_imm(vm, i) ? 1 : 0); }
void op_gt(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) >  Cv_or_imm(vm, i) ? 1 : 0); }
void op_ge(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) >= Cv_or_imm(vm, i) ? 1 : 0); }
void op_lt(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) <  Cv_or_imm(vm, i) ? 1 : 0); }
void op_le(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, Bv(vm, i) <= Cv_or_imm(vm, i) ? 1 : 0); }

/* ===== Время и дата ===== */

void op_time(vm_state_t *vm, uint32_t i) {
    TOD_t t = tm_to_tod(current_tm());
    SetA_val(vm, i, tod_to_seconds(t));
}

void op_date(vm_state_t *vm, uint32_t i) {
    DATE_t d = tm_to_date(current_tm());
    SetA_val(vm, i, d.year*10000 + d.month*100 + d.day);
}

void op_tod(vm_state_t *vm, uint32_t i) {
    TOD_t t = tm_to_tod(current_tm());
    SetA_val(vm, i, t.hours*10000 + t.minutes*100 + t.seconds);
}

void op_dt(vm_state_t *vm, uint32_t i) {
    DT_t dt = tm_to_dt(current_tm());
    SetA_val(vm, i, dt.date.year*1000000 + dt.date.month*10000 + dt.date.day*100 + dt.time.hours); 
}

void op_add_time(vm_state_t *vm, uint32_t i) {
    SetA_val(vm, i, Bv(vm, i) + Cv_or_imm(vm, i));
}

void op_sub_time(vm_state_t *vm, uint32_t i) {
    SetA_val(vm, i, Bv(vm, i) - Cv_or_imm(vm, i));
}

void op_year(vm_state_t *vm, uint32_t i)   { SetA_val(vm, i, tm_to_date(current_tm()).year); }
void op_month(vm_state_t *vm, uint32_t i)  { SetA_val(vm, i, tm_to_date(current_tm()).month); }
void op_day(vm_state_t *vm, uint32_t i)    { SetA_val(vm, i, tm_to_date(current_tm()).day); }
void op_hour(vm_state_t *vm, uint32_t i)   { SetA_val(vm, i, tm_to_tod(current_tm()).hours); }
void op_minute(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, tm_to_tod(current_tm()).minutes); }
void op_second(vm_state_t *vm, uint32_t i) { SetA_val(vm, i, tm_to_tod(current_tm()).seconds); }

/* ===== Строковые функции ===== */

void op_len(vm_state_t *vm, uint32_t i) {
    uint32_t addr = Bv(vm, i);
    if (addr >= VM_MEM_BYTES) { SetA_val(vm, i, 0); return; }
    SetA_val(vm, i, (uint32_t)safe_strlen_at(vm, addr));
}

void op_concat(vm_state_t *vm, uint32_t i) {
    uint32_t destAddr = Bv(vm, i);
    uint32_t srcAddr  = Cv(vm, i);
    if (destAddr >= VM_MEM_BYTES || srcAddr >= VM_MEM_BYTES) return;
    
    size_t dest_len = safe_strlen_at(vm, destAddr);
    size_t src_len = safe_strlen_at(vm, srcAddr);
    size_t rem = safe_mem_remaining(vm, destAddr);
    if (rem == 0) return;
    
    size_t max_append = rem > dest_len ? rem - dest_len - 1 : 0;
    size_t to_copy = src_len < max_append ? src_len : max_append;
    
    if (to_copy > 0) {
        memcpy(vm->mem + destAddr + dest_len, vm->mem + srcAddr, to_copy);
    }
    
    if (destAddr + dest_len + to_copy < VM_MEM_BYTES) {
        vm->mem[destAddr + dest_len + to_copy] = '\0';
    }
}

void op_left(vm_state_t *vm, uint32_t i) {
    uint32_t addr = Bv(vm, i);
    uint32_t n = Cv_or_imm(vm, i);
    if (addr >= VM_MEM_BYTES) return;
    
    size_t len = safe_strlen_at(vm, addr);
    if ((size_t)n >= len) return;
    if (addr + n < VM_MEM_BYTES) vm->mem[addr + n] = '\0';
}

void op_right(vm_state_t *vm, uint32_t i) {
    uint32_t addr = Bv(vm, i);
    uint32_t n = Cv_or_imm(vm, i);
    if (addr >= VM_MEM_BYTES) return;
    
    size_t len = safe_strlen_at(vm, addr);
    if (len == 0) return;
    if ((size_t)n >= len) return;
    
    uint32_t src = addr + (uint32_t)(len - n);
    safe_memmove(vm, addr, src, n);
    if (addr + n < VM_MEM_BYTES) vm->mem[addr + n] = '\0';
}

void op_mid(vm_state_t *vm, uint32_t i) {
    uint32_t addr = Bv(vm, i);
    uint32_t start = Cv_or_imm(vm, i);
    if (addr >= VM_MEM_BYTES) return;
    
    size_t len = safe_strlen_at(vm, addr);
    if ((size_t)start >= len) {
        if (addr < VM_MEM_BYTES) vm->mem[addr] = '\0';
        return;
    }
    
    uint32_t src = addr + start;
    size_t newlen = len - start;
    safe_memmove(vm, addr, src, newlen);
    if (addr + newlen < VM_MEM_BYTES) vm->mem[addr + newlen] = '\0';
}

void op_insert(vm_state_t *vm, uint32_t i) {
    uint32_t destAddr = vm->reg[RA(i)];
    uint32_t insAddr  = Bv(vm, i);
    uint32_t pos      = Cv_or_imm(vm, i);
    if (destAddr >= VM_MEM_BYTES || insAddr >= VM_MEM_BYTES) return;
    
    size_t dest_len = safe_strlen_at(vm, destAddr);
    size_t ins_len = safe_strlen_at(vm, insAddr);
    if ((size_t)pos > dest_len) pos = (uint32_t)dest_len;
    
    size_t rem = safe_mem_remaining(vm, destAddr);
    if (rem == 0) return;
    
    size_t max_insert = rem > dest_len ? rem - dest_len - 1 : 0;
    size_t to_insert = ins_len < max_insert ? ins_len : max_insert;
    
    if (to_insert > 0) {
        size_t tail = dest_len - pos;
        if (destAddr + pos + to_insert + tail + 1 > VM_MEM_BYTES) {
            if (destAddr + pos + to_insert + 1 > VM_MEM_BYTES) return;
        }
        memmove(vm->mem + destAddr + pos + to_insert, vm->mem + destAddr + pos, tail + 1);
        memcpy(vm->mem + destAddr + pos, vm->mem + insAddr, to_insert);
    }
}

void op_delete(vm_state_t *vm, uint32_t i) {
    uint32_t addr = vm->reg[RA(i)];
    uint32_t pos = Bv(vm, i);
    uint32_t n = Cv_or_imm(vm, i);
    if (addr >= VM_MEM_BYTES) return;
    
    size_t len = safe_strlen_at(vm, addr);
    if (pos >= len) return;
    if ((size_t)n > len - pos) n = (uint32_t)(len - pos);
    
    safe_memmove(vm, addr + pos, addr + pos + n, len - pos - n + 1);
}

void op_replace(vm_state_t *vm, uint32_t i) {
    uint32_t addr = vm->reg[RA(i)];
    uint32_t repAddr = Bv(vm, i);
    uint32_t pos = Cv_or_imm(vm, i); 
    if (addr >= VM_MEM_BYTES || repAddr >= VM_MEM_BYTES) return;
    
    size_t len = safe_strlen_at(vm, addr);
    size_t rep_len = safe_strlen_at(vm, repAddr);
    if (pos > len) return;
    
    if (pos + rep_len > len) rep_len = len - pos;
    safe_memmove(vm, addr + pos, addr + pos + rep_len, len - pos - rep_len + 1);
    
    size_t rem = safe_mem_remaining(vm, addr);
    size_t current_len = safe_strlen_at(vm, addr);
    size_t max_insert = rem > current_len ? rem - current_len - 1 : 0;
    size_t to_insert = rep_len < max_insert ? rep_len : max_insert;
    
    if (to_insert > 0) {
        size_t tail = current_len - pos;
        memmove(vm->mem + addr + pos + to_insert, vm->mem + addr + pos, tail + 1);
        memcpy(vm->mem + addr + pos, vm->mem + repAddr, to_insert);
    }
}

/* ===== Таймеры (TON/TOF/TP) ===== */

void op_ton(vm_state_t *vm, uint32_t i) {
    if (!FIMM(i)) return;
    uint8_t id = IMM8(i);
    if (id >= MAX_TIMERS) return;

    bool in = vm->reg[RB(i)] != 0;
    uint32_t pt = vm->reg[RC(i)];

    ton_set(vm, id, in, pt);
    vm->reg[RA(i)] = ton_Q(vm, id) ? 1 : 0;
}

void op_tof(vm_state_t *vm, uint32_t i) {
    if (!FIMM(i)) return;
    uint8_t id = IMM8(i);
    if (id >= MAX_TIMERS) return;

    bool in = vm->reg[RB(i)] != 0;
    uint32_t pt = vm->reg[RC(i)];

    tof_set(vm, id, in, pt);
    vm->reg[RA(i)] = tof_Q(vm, id) ? 1 : 0;
}

void op_tp(vm_state_t *vm, uint32_t i) {
    if (!FIMM(i)) return;
    uint8_t id = IMM8(i);
    if (id >= MAX_TIMERS) return;

    bool in = vm->reg[RB(i)] != 0;
    uint32_t pt = vm->reg[RC(i)];

    tp_set(vm, id, in, pt);
    vm->reg[RA(i)] = tp_Q(vm, id) ? 1 : 0;
}

/* ===== Счётчики CTU/CTD/CTUD ===== */

#ifdef UNIT_TEST
void vm_counters_reset(vm_state_t *vm) {
    for (int i = 0; i < MAX_TIMERS; ++i) {
        vm->ctu_counters[i].value = 0;
        vm->ctu_counters[i].preset = 0;
        vm->ctd_counters[i].value = 0;
        vm->ctd_counters[i].preset = 0;
        vm->ctud_counters[i].value = 0;
        vm->ctud_counters[i].preset = 0;
        
        vm->ctu_prev_input[i] = false;
        vm->ctd_prev_input[i] = false;
        vm->ctud_prev_up[i] = false;
        vm->ctud_prev_down[i] = false;
    }
}
#endif

void op_ctu(vm_state_t *vm, uint32_t i) {
    int id = (int)vm->reg[RA(i)];
    if (id < 0 || id >= MAX_TIMERS) return;
    
    bool in = vm->reg[RB(i)] != 0;
    uint32_t preset = Cv_or_imm(vm, i);
    
    bool rising = in && !vm->ctu_prev_input[id];
    if (rising) {
        if (vm->ctu_counters[id].value < UINT32_MAX) {
            vm->ctu_counters[id].value++;
        }
    }
    vm->ctu_prev_input[id] = in;
    
    vm->reg[RA(i)] = (vm->ctu_counters[id].value >= preset) ? 1 : 0;
}

void op_ctd(vm_state_t *vm, uint32_t i) {
    int id = (int)vm->reg[RA(i)];
    if (id < 0 || id >= MAX_TIMERS) return;
    
    bool in = vm->reg[RB(i)] != 0;
    uint32_t preset = Cv_or_imm(vm, i);
    
    bool rising = in && !vm->ctd_prev_input[id];
    if (rising) {
        if (vm->ctd_counters[id].value > 0) {
            vm->ctd_counters[id].value--;
        }
    }
    vm->ctd_prev_input[id] = in;
    
    vm->reg[RA(i)] = (vm->ctd_counters[id].value <= preset) ? 1 : 0;
}

void op_ctud(vm_state_t *vm, uint32_t i) {
    int id = (int)vm->reg[RA(i)];
    if (id < 0 || id >= MAX_TIMERS) return;
    
    bool up = vm->reg[RB(i)] != 0;
    bool down = vm->reg[RC(i)] != 0;
    
    bool up_rising = up && !vm->ctud_prev_up[id];
    bool down_rising = down && !vm->ctud_prev_down[id];
    
    if (up_rising) {
        if (vm->ctud_counters[id].value < UINT32_MAX) {
            vm->ctud_counters[id].value++;
        }
    }
    if (down_rising) {
        if (vm->ctud_counters[id].value > 0) {
            vm->ctud_counters[id].value--;
        }
    }
    
    vm->ctud_prev_up[id] = up;
    vm->ctud_prev_down[id] = down;
    
    vm->reg[RA(i)] = vm->ctud_counters[id].value;
}

/* ===== limit / sel / mux ===== */

void op_limit(vm_state_t *vm, uint32_t i) {
    uint32_t x = vm->reg[RA(i)];
    uint32_t lo = Bv(vm, i);
    uint32_t hi = Cv_or_imm(vm, i);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    SetA_val(vm, i, x);
}

void op_sel(vm_state_t *vm, uint32_t i) {
    uint32_t cond = Bv(vm, i);
    uint32_t base = vm->reg[RC(i)];
    if (base >= VM_MEM_BYTES) { SetA_val(vm, i, 0); return; }
    
    uint32_t v1 = 0, v2 = 0;
    if (base + 4 <= VM_MEM_BYTES) v1 = vm_mr32(vm, base);
    if (base + 8 <= VM_MEM_BYTES) v2 = vm_mr32(vm, base + 4);
    SetA_val(vm, i, cond ? v1 : v2);
}

void op_mux(vm_state_t *vm, uint32_t i) {
    uint32_t base = Bv(vm, i);
    uint32_t idx = Cv_or_imm(vm, i);
    uint64_t addr = (uint64_t)base + (uint64_t)idx * 4ULL;
    if (addr + 4 > VM_MEM_BYTES) { SetA_val(vm, i, 0); return; }
    SetA_val(vm, i, vm_mr32(vm, (uint32_t)addr));
}

/* ===== IEC/SCADA: Edge detectors, latches, demux ===== */

/*
 * id selection convention:
 *   if (FIMM(i)) id = IMM8(i)
 *   else id = (int)vm->reg[RA(i)]
 *
 * input signal is read from Bv(vm, i) (boolean: non-zero => true)
 * outputs are written to register A (SetA_val)
 */

/* Rising edge: output 1 when input goes 0->1 for the given id */
void op_rising_edge(vm_state_t *vm, uint32_t i) {
    int id = -1;
    if (FIMM(i)) {
        id = (int)IMM8(i);
    } else {
        id = (int)vm->reg[RA(i)];
    }
    if (id < 0 || id >= MAX_TIMERS) { SetA_val(vm, i, 0); return; }

    bool in = Bv(vm, i) != 0;
    bool prev = vm->edge_prev_input[id];
    bool rising = in && !prev;
    vm->edge_prev_input[id] = in;
    SetA_val(vm, i, rising ? 1 : 0);
}

/* Falling edge: output 1 when input goes 1->0 for the given id */
void op_falling_edge(vm_state_t *vm, uint32_t i) {
    int id = -1;
    if (FIMM(i)) {
        id = (int)IMM8(i);
    } else {
        id = (int)vm->reg[RA(i)];
    }
    if (id < 0 || id >= MAX_TIMERS) { SetA_val(vm, i, 0); return; }

    bool in = Bv(vm, i) != 0;
    bool prev = vm->edge_prev_input[id];
    bool falling = !in && prev;
    vm->edge_prev_input[id] = in;
    SetA_val(vm, i, falling ? 1 : 0);
}

/* Both edges: output 1 when input changes (either rising or falling) */
void op_edge_both(vm_state_t *vm, uint32_t i) {
    int id = -1;
    if (FIMM(i)) {
        id = (int)IMM8(i);
    } else {
        id = (int)vm->reg[RA(i)];
    }
    if (id < 0 || id >= MAX_TIMERS) { SetA_val(vm, i, 0); return; }

    bool in = Bv(vm, i) != 0;
    bool prev = vm->edge_prev_input[id];
    bool changed = (in != prev);
    vm->edge_prev_input[id] = in;
    SetA_val(vm, i, changed ? 1 : 0);
}

/*
 * RS / SR latches
 *
 * Calling convention:
 *   id = FIMM ? IMM8(i) : (int)vm->reg[RA(i)]
 *   S input = vm->reg[RB(i)] != 0
 *   R input = vm->reg[RC(i)] != 0
 *   result written back to vm->reg[RA(i)] (1 or 0)
 *
 * Note: this follows common PLC style where RA contains the id and is overwritten
 * with the Q value (like many other ops in this codebase).
 */

/* RS latch: Reset has priority (if R==1 => Q=0, else if S==1 => Q=1, else keep) */
void op_rs_latch(vm_state_t *vm, uint32_t i) {
    int id = -1;
    if (FIMM(i)) {
        id = (int)IMM8(i);
    } else {
        id = (int)vm->reg[RA(i)];
    }
    if (id < 0 || id >= MAX_TIMERS) { SetA_val(vm, i, 0); return; }

    bool S = vm->reg[RB(i)] != 0;
    bool R = vm->reg[RC(i)] != 0;

    if (R) {
        vm->rs_latches[id] = false;
    } else if (S) {
        vm->rs_latches[id] = true;
    }
    SetA_val(vm, i, vm->rs_latches[id] ? 1 : 0);
}

/* SR latch: Set has priority (if S==1 => Q=1, else if R==1 => Q=0, else keep) */
void op_sr_latch(vm_state_t *vm, uint32_t i) {
    int id = -1;
    if (FIMM(i)) {
        id = (int)IMM8(i);
    } else {
        id = (int)vm->reg[RA(i)];
    }
    if (id < 0 || id >= MAX_TIMERS) { SetA_val(vm, i, 0); return; }

    bool S = vm->reg[RB(i)] != 0;
    bool R = vm->reg[RC(i)] != 0;

    if (S) {
        vm->sr_latches[id] = true;
    } else if (R) {
        vm->sr_latches[id] = false;
    }
    SetA_val(vm, i, vm->sr_latches[id] ? 1 : 0);
}

/*
 * Demultiplexer:
 *   baseAddr = Bv(vm,i)  (memory base address of outputs array)
 *   idx      = Cv_or_imm(vm,i)  (index to select)
 *   value    = vm->reg[RA(i)]   (32-bit value to write)
 *
 * Writes 32-bit value to baseAddr + idx*4, if in bounds.
 */
void op_demux(vm_state_t *vm, uint32_t i) {
    uint32_t base = Bv(vm, i);
    uint32_t idx = Cv_or_imm(vm, i);
    /* compute address as 64-bit to avoid overflow */
    uint64_t addr = (uint64_t)base + (uint64_t)idx * 4ULL;
    if (addr + 4 > VM_MEM_BYTES) return;
    vm_mw32(vm, (uint32_t)addr, vm->reg[RA(i)]);
}

/* ===== JMP-инструкции ===== */

void op_jmp(vm_state_t *vm, uint32_t i) {
    vm->PC = FIMM(i) ? SEXTIMM8(i) : Bv(vm, i);
}

void op_jmp_if(vm_state_t *vm, uint32_t i) {
    if (Cv(vm, i)) {
        vm->PC = FIMM(i) ? SEXTIMM8(i) : Bv(vm, i);
    }
}

void op_jmp_if_not(vm_state_t *vm, uint32_t i) {
    if (!Cv(vm, i)) {
        vm->PC = FIMM(i) ? SEXTIMM8(i) : Bv(vm, i);
    }
}

/* ===== Управляющие инструкции ===== */

void op_halt(vm_state_t *vm, uint32_t i) {
    (void)i;
    vm->running = false;
    atomic_store(&vm->stop_requested, true);
}

void op_nop(vm_state_t *vm, uint32_t i) {
    (void)i;
}

void op_exit(vm_state_t *vm, uint32_t i) {
    int code = 0;
    
    if (Av(vm, i) == 0) {
        code = (int)IMM8(i);
    } else {
        code = (int)A(i);
    }

    vm->exit_code = code;
    vm->running = false;
    atomic_store(&vm->stop_requested, true);
}