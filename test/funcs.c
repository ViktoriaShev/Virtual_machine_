#define _POSIX_C_SOURCE 199309L
#include "vm32.h"
#include "funcs.h"
#include "timers.h"
// funcs.c -- реализация инструкций VM (строки, таймеры, счётчики, арифметика и т.д.)
#include <string.h>
#include <stdio.h>
#include <strings.h> 

TON_Timer ton_timers[16] = {0};
TOF_Timer tof_timers[16] = {0};
TP_Timer tp_timers[16] = {0};

CT_Counter ctu_counters[16] = {0};
CT_Counter ctd_counters[16] = {0};
CT_Counter ctud_counters[16] = {0};

/* --- Вспомогательные функции безопасности для работы со строками/памятью --- */

static inline size_t safe_mem_remaining(uint32_t addr) {
    if (addr >= MEM_BYTES) return 0;
    return (size_t)(MEM_BYTES - addr);
}

/* strlen, но ограниченная рамками памяти VM */
static size_t safe_strlen_at(uint32_t addr) {
    size_t rem = safe_mem_remaining(addr);
    size_t i = 0;
    if (rem == 0) return 0;
    while (i < rem && mem[addr + i] != '\0') i++;
    return i;
}

/* копирует n байт из srcAddr в destAddr, с учётом границ VM */
static size_t safe_memcpy_from_to(uint32_t destAddr, const uint8_t *src, size_t n) {
    size_t rem = safe_mem_remaining(destAddr);
    size_t to_copy = n;
    if (to_copy > rem) to_copy = rem;
    if (to_copy > 0) memcpy(mem + destAddr, src, to_copy);
    return to_copy;
}

/* memmove внутри mem (байтовая) с учетом границ */
static void safe_memmove(uint32_t destAddr, uint32_t srcAddr, size_t n) {
    size_t rem_dest = safe_mem_remaining(destAddr);
    size_t rem_src = safe_mem_remaining(srcAddr);
    size_t max_copy = rem_dest < rem_src ? rem_dest : rem_src;
    if (n > max_copy) n = max_copy;
    memmove(mem + destAddr, mem + srcAddr, n);
}

/* записать нультерминатор, если адрес валиден */
static void safe_set_null(uint32_t addr) {
    if (addr < MEM_BYTES) mem[addr] = 0;
}


// Возвращает локальное время
static struct tm current_tm(void) {
    time_t t = time(NULL);
    struct tm tm_now;
    localtime_r(&t, &tm_now); // потокобезопасно
    return tm_now;
}

// Конверсия struct tm -> TOD_t
static TOD_t tm_to_tod(struct tm t) {
    TOD_t tod;
    tod.hours = t.tm_hour;
    tod.minutes = t.tm_min;
    tod.seconds = t.tm_sec;
    return tod;
}

// Конверсия struct tm -> DATE_t
static DATE_t tm_to_date(struct tm t) {
    DATE_t date;
    date.year = t.tm_year + 1900;
    date.month = t.tm_mon + 1;
    date.day = t.tm_mday;
    return date;
}

// Конверсия struct tm -> DT_t
static DT_t tm_to_dt(struct tm t) {
    DT_t dt;
    dt.date = tm_to_date(t);
    dt.time = tm_to_tod(t);
    return dt;
}

// Преобразование TOD_t в секунды с начала дня
static int32_t tod_to_seconds(TOD_t t) {
    return t.hours*3600 + t.minutes*60 + t.seconds;
}

/* ----- арифметика (целочисленная) ----- */
void op_add(uint32_t i) { SetA_val(i, Bv(i) + Cv(i)); }
void op_sub(uint32_t i) { SetA_val(i, Bv(i) - Cv(i)); }
void op_mul(uint32_t i) { SetA_val(i, Bv(i) * Cv(i)); }
void op_div(uint32_t i) { uint32_t c = Cv(i); SetA_val(i, c ? Bv(i)/c : 0); }
void op_mod(uint32_t i) { uint32_t c = Cv(i); SetA_val(i, c ? Bv(i)%c : 0); }

/* Математические функции используют double -> результат приводим к uint32_t. */
void op_expt(uint32_t i) { SetA_val(i, (uint32_t)pow((double)Bv(i),(double)Cv(i))); }
void op_abs(uint32_t i)  { SetA_val(i, (uint32_t)abs((int32_t)Bv(i))); }
void op_sqrt(uint32_t i) { SetA_val(i, (uint32_t)sqrt((double)Bv(i))); }
void op_ln(uint32_t i)   { SetA_val(i, (uint32_t)log((double)Bv(i))); }
void op_log(uint32_t i)  { SetA_val(i, (uint32_t)log10((double)Bv(i))); }
void op_exp(uint32_t i)  { SetA_val(i, (uint32_t)exp((double)Bv(i))); }
void op_sin(uint32_t i)  { SetA_val(i, (uint32_t)sin((double)Bv(i))); }
void op_cos(uint32_t i)  { SetA_val(i, (uint32_t)cos((double)Bv(i))); }
void op_tan(uint32_t i)  { SetA_val(i, (uint32_t)tan((double)Bv(i))); }
void op_asin(uint32_t i) { SetA_val(i, (uint32_t)asin((double)Bv(i))); }
void op_acos(uint32_t i) { SetA_val(i, (uint32_t)acos((double)Bv(i))); }
void op_atan(uint32_t i) { SetA_val(i, (uint32_t)atan((double)Bv(i))); }

/* ----- логика ----- */
void op_and(uint32_t i) { SetA_val(i, Bv(i) & Cv(i)); }
void op_or(uint32_t i)  { SetA_val(i, Bv(i) | Cv(i)); }
void op_xor(uint32_t i) { SetA_val(i, Bv(i) ^ Cv(i)); }
void op_not(uint32_t i) { SetA_val(i, ~Bv(i)); }

/* ----- сравнения ----- */
// все операции сравнения записывают результат прямо в регистр A
void op_eq(uint32_t i) { SetA_val(i, Bv(i) == Cv(i) ? 1 : 0); }
void op_ne(uint32_t i) { SetA_val(i, Bv(i) != Cv(i) ? 1 : 0); }
void op_gt(uint32_t i) { SetA_val(i, Bv(i) >  Cv(i) ? 1 : 0); }
void op_ge(uint32_t i) { SetA_val(i, Bv(i) >= Cv(i) ? 1 : 0); }
void op_lt(uint32_t i) { SetA_val(i, Bv(i) <  Cv(i) ? 1 : 0); }
void op_le(uint32_t i) { SetA_val(i, Bv(i) <= Cv(i) ? 1 : 0); }


/* ----- время / дата (заглушки) ----- */
// ----- Время и дата -----
void op_time(uint32_t i) {
    TOD_t t = tm_to_tod(current_tm());
    SetA_val(i, tod_to_seconds(t)); // секунды с начала дня
}

void op_date(uint32_t i) {
    DATE_t d = tm_to_date(current_tm());
    // Возвращаем YYYYMMDD
    SetA_val(i, d.year*10000 + d.month*100 + d.day);
}

void op_tod(uint32_t i) {
    TOD_t t = tm_to_tod(current_tm());
    SetA_val(i, t.hours*10000 + t.minutes*100 + t.seconds); // HHMMSS
}

void op_dt(uint32_t i) {
    DT_t dt = tm_to_dt(current_tm());
    SetA_val(i, dt.date.year*1000000 + dt.date.month*10000 + dt.date.day*100 + dt.time.hours); 
    // усечённо, можно расширить до uint64, если нужно
}

void op_add_time(uint32_t i) {
    SetA_val(i, Bv(i) + Cv(i));
}

void op_sub_time(uint32_t i) {
    SetA_val(i, Bv(i) - Cv(i));
}

// Отдельные компоненты даты/времени
void op_year(uint32_t i)   { SetA_val(i, tm_to_date(current_tm()).year); }
void op_month(uint32_t i)  { SetA_val(i, tm_to_date(current_tm()).month); }
void op_day(uint32_t i)    { SetA_val(i, tm_to_date(current_tm()).day); }
void op_hour(uint32_t i)   { SetA_val(i, tm_to_tod(current_tm()).hours); }
void op_minute(uint32_t i) { SetA_val(i, tm_to_tod(current_tm()).minutes); }
void op_second(uint32_t i) { SetA_val(i, tm_to_tod(current_tm()).seconds); }

/* ===== Строковые функции (безопасно, байтовая память) ===== */


// запись нуль-терминированной строки из C в память VM
// адрес dest берётся из регистра A, длина строки = B, источник строки из reg[C]
void op_write_string(uint32_t instr) {
    uint32_t destAddr = reg[RA(instr)];  // регистр A — адрес
    uint32_t srcAddr  = reg[RB(instr)];  // регистр B — адрес исходной строки в памяти VM
    uint32_t maxLen   = RC(instr);       // C — максимальная длина записи
    if (destAddr >= MEM_BYTES || srcAddr >= MEM_BYTES) return;

    size_t src_len = safe_strlen_at(srcAddr);
    size_t rem = safe_mem_remaining(destAddr);
    size_t to_copy = src_len;
    if (to_copy > rem - 1) to_copy = rem - 1;     // оставляем место для '\0'
    if (to_copy > maxLen) to_copy = maxLen;

    memcpy(mem + destAddr, mem + srcAddr, to_copy);
    mem[destAddr + to_copy] = '\0';
}

// запись строки напрямую из встроенного буфера C
void op_write_const(uint32_t instr, const char *s) {
    uint32_t destAddr = reg[RA(instr)];  // регистр A — адрес
    if (!s || destAddr >= MEM_BYTES) return;

    size_t len = strlen(s);
    size_t rem = safe_mem_remaining(destAddr);
    if (len > rem - 1) len = rem - 1;

    memcpy(mem + destAddr, s, len);
    mem[destAddr + len] = '\0';
}


/* Возвращает длину строки по адресу в регистре B */
void op_len(uint32_t i) {
    uint32_t addr = Bv(i);
    if (addr >= MEM_BYTES) { SetA_val(i, 0); return; }
    SetA_val(i, (uint32_t)safe_strlen_at(addr));
}

/* Конкатенация: destAddr = B, srcAddr = C. Результат помещается в dest (in-place). */
void op_concat(uint32_t i) {
    uint32_t destAddr = Bv(i);
    uint32_t srcAddr  = Cv(i);
    if (destAddr >= MEM_BYTES || srcAddr >= MEM_BYTES) return;
    size_t dest_len = safe_strlen_at(destAddr);
    size_t src_len = safe_strlen_at(srcAddr);
    size_t rem = safe_mem_remaining(destAddr);
    if (rem == 0) return;
    size_t max_append = rem > dest_len ? rem - dest_len - 1 : 0;
    size_t to_copy = src_len < max_append ? src_len : max_append;
    if (to_copy > 0) {
        memcpy(mem + destAddr + dest_len, mem + srcAddr, to_copy);
    }
    /* нуль-терминатор */
    if (destAddr + dest_len + to_copy < MEM_BYTES) mem[destAddr + dest_len + to_copy] = '\0';
}

/* LEFT: первые n символов строки по адресу B, in-place */
void op_left(uint32_t i) {
    uint32_t addr = Bv(i);
    uint32_t n = Cv(i);
    if (addr >= MEM_BYTES) return;
    size_t len = safe_strlen_at(addr);
    if ((size_t)n >= len) return; /* ничего не делаем */
    /* сдвигаем первые n байт в начало (они уже там) и ставим '\0' */
    if (addr + n < MEM_BYTES) mem[addr + n] = '\0';
}

/* RIGHT: последние n символов строки по адресу B, in-place */
void op_right(uint32_t i) {
    uint32_t addr = Bv(i);
    uint32_t n = Cv(i);
    if (addr >= MEM_BYTES) return;
    size_t len = safe_strlen_at(addr);
    if (len == 0) return;
    if ((size_t)n >= len) { /* оставляем всю строку */
        return;
    }
    uint32_t src = addr + (uint32_t)(len - n);
    /* memmove на себя */
    safe_memmove(addr, src, n);
    if (addr + n < MEM_BYTES) mem[addr + n] = '\0';
}

/* MID: обрезает строку, чтобы она начиналась с offset C (in-place):
   mid(s, start) => s := s[start..end] */
void op_mid(uint32_t i) {
    uint32_t addr = Bv(i);
    uint32_t start = Cv(i);
    if (addr >= MEM_BYTES) return;
    size_t len = safe_strlen_at(addr);
    if ((size_t)start >= len) { /* результирующая пустая строка */
        if (addr < MEM_BYTES) mem[addr] = '\0';
        return;
    }
    uint32_t src = addr + start;
    size_t newlen = len - start;
    safe_memmove(addr, src, newlen);
    if (addr + newlen < MEM_BYTES) mem[addr + newlen] = '\0';
}

/* INSERT: в строку по адресу reg[RA] вставить текст из строки по адресу reg[RB] в позицию pos=C */
void op_insert(uint32_t i) {
    uint32_t destAddr = reg[RA(i)];
    uint32_t insAddr  = Bv(i); // reg[RB]
    uint32_t pos      = Cv(i);
    if (destAddr >= MEM_BYTES || insAddr >= MEM_BYTES) return;
    size_t dest_len = safe_strlen_at(destAddr);
    size_t ins_len = safe_strlen_at(insAddr);
    if ((size_t)pos > dest_len) pos = (uint32_t)dest_len;
    size_t rem = safe_mem_remaining(destAddr);
    if (rem == 0) return;
    /* сколько свободно для вставки (включая nul) */
    size_t max_insert = rem > dest_len ? rem - dest_len - 1 : 0;
    size_t to_insert = ins_len < max_insert ? ins_len : max_insert;
    /* сдвинем хвост вправо */
    if (to_insert > 0) {
        size_t tail = dest_len - pos;
        /* убедимся, что destAddr+pos+to_insert+tail <= MEM_BYTES */
        if (destAddr + pos + to_insert + tail + 1 > MEM_BYTES) {
            /* уменьшим tail/insert, но проще — отрежем вставку */
            if (destAddr + pos + to_insert + 1 > MEM_BYTES) {
                /* нечего вставить */
                return;
            }
        }
        /* memmove вправо */
        memmove(mem + destAddr + pos + to_insert, mem + destAddr + pos, tail + 1); /* +1 чтобы скопировать '\0' */
        memcpy(mem + destAddr + pos, mem + insAddr, to_insert);
    }
}

/* DELETE: удалить n=C символов с позиции pos=B в строке по адресу reg[RA] */
void op_delete(uint32_t i) {
    uint32_t addr = reg[RA(i)];
    uint32_t pos = Bv(i);
    uint32_t n = Cv(i);
    if (addr >= MEM_BYTES) return;
    size_t len = safe_strlen_at(addr);
    if (pos >= len) return;
    if ((size_t)n > len - pos) n = (uint32_t)(len - pos);
    safe_memmove(addr + pos, addr + pos + n, len - pos - n + 1); /* +1 чтобы копировать '\0' */
}

/* REPLACE: в строке по адресу reg[RA] заменить n=C символов с позиции pos=B на строку по адресу reg[RB] */
void op_replace(uint32_t i) {
    uint32_t addr = reg[RA(i)];
    uint32_t repAddr = Bv(i);
    uint32_t pos = Cv(i);
    if (addr >= MEM_BYTES || repAddr >= MEM_BYTES) return;
    size_t len = safe_strlen_at(addr);
    size_t rep_len = safe_strlen_at(repAddr);
    if (pos > len) return;
    /* удаляем часть длины rep_len (или C?) — здесь я трактую C как число символов, которые заменяем.
       Но т.к. у нас C используется как pos, для простоты: заменяем одну подстроку длиной rep_len */
    /* Для простоты: выполним delete(pos, rep_len) затем insert(rep at pos). */
    /* Удаляем tail, затем вставляем rep — используем существующие helper'ы: */
    /* delete */
    if (pos + rep_len > len) rep_len = len - pos;
    safe_memmove(addr + pos, addr + pos + rep_len, len - pos - rep_len + 1);
    /* insert: вставим repAddr (ограничено памятью) */
    /* shift right to make space */
    size_t rem = safe_mem_remaining(addr);
    size_t current_len = safe_strlen_at(addr);
    size_t max_insert = rem > current_len ? rem - current_len - 1 : 0;
    size_t to_insert = rep_len < max_insert ? rep_len : max_insert;
    if (to_insert > 0) {
        size_t tail = current_len - pos;
        memmove(mem + addr + pos + to_insert, mem + addr + pos, tail + 1);
        memcpy(mem + addr + pos, mem + repAddr, to_insert);
    }
}

/* ===== Таймеры (TON/TOF/TP) — IEC-style, с детекцией фронтов ===== */

/* prev input arrays для детектирования фронтов */
static bool ton_prev_input[16] = {0};
static bool tof_prev_input[16] = {0};
static bool tp_prev_input[16]  = {0};

/* Helper: получить timer id из регистра A (значение в регистре) */
static inline int get_timer_id_from_regA(uint32_t i) {
    uint32_t possible = reg[RA(i)];
    if (possible >= 16) return -1;
    return (int)possible;
}

void op_ton(uint32_t i) {
    uint8_t id = reg[RA(i)];
    bool in = reg[RB(i)] != 0;
    uint32_t pt = reg[RC(i)];
    ton_set(id, in, pt);
    reg[RA(i)] = ton_Q(id);
}

void op_tof(uint32_t i) {
    uint8_t id = reg[RA(i)];
    bool in = reg[RB(i)] != 0;
    uint32_t pt = reg[RC(i)];
    tof_set(id, in, pt);
    reg[RA(i)] = tof_Q(id);
}

void op_tp(uint32_t i) {
    uint8_t id = reg[RA(i)];
    bool in = reg[RB(i)] != 0;
    uint32_t pt = reg[RC(i)];
    tp_set(id, in, pt);
    reg[RA(i)] = tp_Q(id);
}

/* ===== Счётчики CTU/CTD/CTUD ===== */

/* prev inputs для CU/CD */
static bool ctu_prev[16] = {0};
static bool ctd_prev[16] = {0};

/* CTU: регистр A содержит индекс счётчика (в reg[RA]), вход CU in reg[RB] */
void op_ctu(uint32_t i) {
    uint32_t idx = reg[RA(i)];
    if (idx >= 16) return;
    bool in = (reg[RB(i)] != 0);
    if (in && !ctu_prev[idx]) {
        ctu_counters[idx].value++;
    }
    ctu_prev[idx] = in;
    reg[RA(i)] = ctu_counters[idx].value;
}

/* CTD: регистр A содержит индекс счётчика (в reg[RA]), вход CD in reg[RB] */
void op_ctd(uint32_t i) {
    uint32_t idx = reg[RA(i)];
    if (idx >= 16) return;
    bool in = (reg[RB(i)] != 0);
    if (in && !ctd_prev[idx]) {
        if (ctd_counters[idx].value > 0) ctd_counters[idx].value--;
    }
    ctd_prev[idx] = in;
    reg[RA(i)] = ctd_counters[idx].value;
}

/* CTUD: A contains counter index (reg[RA]), RB is CU input, RC is CD input */
void op_ctud(uint32_t i) {
    uint32_t idx = reg[RA(i)];
    if (idx >= 16) return;
    bool cu = (reg[RB(i)] != 0);
    bool cd = (reg[RC(i)] != 0);

    /* detect rising edges */
    if (cu && !ctu_prev[idx]) ctu_counters[idx].value++;
    if (cd && !ctd_prev[idx]) {
        if (ctd_counters[idx].value < 0xFFFFFFFFu) ctd_counters[idx].value++;
    }
    ctu_prev[idx] = cu;
    ctd_prev[idx] = cd;

    /* reg gets difference (up - down) */
    uint32_t up = ctu_counters[idx].value;
    uint32_t down = ctd_counters[idx].value;
    reg[RA(i)] = (up >= down) ? (up - down) : 0; /* saturate at 0 */
}

/* ===== limit / sel / mux (используют mr32 для чтения 32-bit слов в памяти) ===== */

void op_limit(uint32_t i) {
    uint32_t x = reg[RA(i)];
    uint32_t lo = Bv(i);
    uint32_t hi = Cv(i);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    SetA_val(i, x);
}

/* SEL: условие в B (значение), base address в reg[RC], возвращает либо mr32(base) либо mr32(base+4) */
void op_sel(uint32_t i) {
    uint32_t cond = Bv(i);
    uint32_t base = reg[RC(i)];
    if (base >= MEM_BYTES) { SetA_val(i, 0); return; }
    uint32_t v1 = 0, v2 = 0;
    if (base + 4 <= MEM_BYTES) v1 = mr32(base);
    if (base + 8 <= MEM_BYTES) v2 = mr32(base + 4);
    SetA_val(i, cond ? v1 : v2);
}

/* MUX: base address in B, idx in C (index of 32-bit word), returns mr32(base + idx*4) */
void op_mux(uint32_t i) {
    uint32_t base = Bv(i);
    uint32_t idx = Cv(i);
    uint64_t addr = (uint64_t)base + (uint64_t)idx * 4ULL;
    if (addr + 4 > MEM_BYTES) { SetA_val(i, 0); return; }
    SetA_val(i, mr32((uint32_t)addr));
}

// JMP-инструкции теперь изменяют глобальную переменную PC
void op_jmp(uint32_t i) {
    PC = Bv(i);
}

void op_jmp_if(uint32_t i) {
    if (Cv(i)) {
        PC = Bv(i);
    }
}

void op_jmp_if_not(uint32_t i) {
    if (!Cv(i)) {
        PC = Bv(i);
    }
}
/* ===== Управляющие инструкции ===== */


// HALT - останавливает VM
void op_halt(uint32_t i) {
    (void)i; // не используется
    running = false;
}

// NOP - ничего не делает
void op_nop(uint32_t i) {
    (void)i;
}