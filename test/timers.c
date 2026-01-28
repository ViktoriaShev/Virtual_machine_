#define _POSIX_C_SOURCE 199309L
#include "timers.h"
#include "vm32.h"
#include <string.h>

/* уже имеются: ton_timers, tof_timers, tp_timers ... (они определены здесь) */
IEC_Timer ton_timers[MAX_TIMERS];
IEC_Timer tof_timers[MAX_TIMERS];
IEC_Timer tp_timers[MAX_TIMERS];
IEC_Timer tonr_timers[MAX_TIMERS];
IEC_Timer tofr_timers[MAX_TIMERS];

static inline uint32_t ms_diff(struct timespec *a, struct timespec *b) {
    return (uint32_t)((b->tv_sec - a->tv_sec) * 1000ULL +
                      (b->tv_nsec - a->tv_nsec) / 1000000ULL);
}

void timers_init(void) {
    memset(ton_timers, 0, sizeof(ton_timers));
    memset(tof_timers, 0, sizeof(tof_timers));
    memset(tp_timers, 0, sizeof(tp_timers));
    memset(tonr_timers, 0, sizeof(tonr_timers));
    memset(tofr_timers, 0, sizeof(tofr_timers));
}
/* --- IEC 61131-3 Logics ------------------------------------------------ */

static void update_ton(IEC_Timer *t) {
    if (!t->enabled || t->preset_ms == 0) {
        t->output = false;
        t->ET = 0;
        return;
    }

    bool rising = (t->input && !t->prev_input);

    if (t->input) {
        if (rising) {
            t->start.tv_sec = (time_t)(time_ms / 1000ULL);
            t->start.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
            t->timing = true;
        }
        if (t->timing) {
            struct timespec now;
            now.tv_sec = (time_t)(time_ms / 1000ULL);
            now.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
            t->ET = ms_diff(&t->start, &now);
            if (t->ET >= t->preset_ms) {
                t->output = true;
                t->timing = false;
            }
        }
    } else {
        t->output = false;
        t->ET = 0;
        t->timing = false;
    }

    t->prev_input = t->input;
}

static void update_tof(IEC_Timer *t) {
    if (!t->enabled || t->preset_ms == 0) {
        t->output = false;
        t->ET = 0;
        return;
    }

    bool falling = (!t->input && t->prev_input);

    if (t->input) {
        t->output = true;
        t->timing = false;
        t->ET = 0;
    } else {
         if (falling) {
            t->start.tv_sec = (time_t)(time_ms / 1000ULL);
            t->start.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
            t->timing = true;
        }

        if (t->timing) {
            struct timespec now;
            now.tv_sec = (time_t)(time_ms / 1000ULL);
            now.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
            t->ET = ms_diff(&t->start, &now);
            if (t->ET >= t->preset_ms) {
                t->output = false;
                t->timing = false;
            } else {
                t->output = true;
            }
        }
    }

    t->prev_input = t->input;
}

static void update_tp(IEC_Timer *t) {
    if (!t->enabled || t->preset_ms == 0) {
        t->output = false;
        t->ET = 0;
        return;
    }

    bool rising = (t->input && !t->prev_input);

    if (rising) {
        t->start.tv_sec = (time_t)(time_ms / 1000ULL);
        t->start.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
        t->timing = true;
        t->output = true;
    }

    if (t->timing) {
        struct timespec now;
        now.tv_sec = (time_t)(time_ms / 1000ULL);
        now.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
        t->ET = ms_diff(&t->start, &now);
        if (t->ET >= t->preset_ms) {
            t->output = false;
            t->timing = false;
        }
    }

    t->prev_input = t->input;
}

static void update_tonr(IEC_Timer *t) {  /* Ретентивный TON */
    if (!t->enabled) return;

    bool rising = (t->input && !t->prev_input);

    if (t->input) {
        if (rising) {
            t->start.tv_sec = (time_t)(time_ms / 1000ULL);
            t->start.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
            t->timing = true;
        }
        struct timespec now;
        now.tv_sec = (time_t)(time_ms / 1000ULL);
        now.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
        if (t->timing) {
            uint32_t add = ms_diff(&t->start, &now);
            t->ET += add;
            t->start.tv_sec = now.tv_sec;
            t->start.tv_nsec = now.tv_nsec;
         }
        
        if (t->ET >= t->preset_ms) {
            t->output = true;
            t->timing = false;
        }
    }

    t->prev_input = t->input;
}

static void update_tofr(IEC_Timer *t) {  /* Ретентивный TOF */
    if (!t->enabled) return;

    bool falling = (!t->input && t->prev_input);

    if (falling) {
        t->start.tv_sec = (time_t)(time_ms / 1000ULL);
        t->start.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
        t->timing = true;
    }

    if (t->timing) {
        struct timespec now;
        now.tv_sec = (time_t)(time_ms / 1000ULL);
        now.tv_nsec = (long)((time_ms % 1000ULL) * 1000000ULL);
        uint32_t add = ms_diff(&t->start, &now);
        t->ET += add;
        t->start.tv_sec = now.tv_sec;
        t->start.tv_nsec = now.tv_nsec;

        if (t->ET >= t->preset_ms) {
            t->output = false;
            t->timing = false;
        } else {
            t->output = true;
        }
    }

    if (t->input) {
        t->output = true;
        t->timing = false;
        t->ET = 0;
    }

    t->prev_input = t->input;
}

/* --- Общий апдейтер --------------------------------------------------- */

void update_all_timers(void) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        update_ton(&ton_timers[i]);
        update_tof(&tof_timers[i]);
        update_tp(&tp_timers[i]);
        update_tonr(&tonr_timers[i]);
        update_tofr(&tofr_timers[i]);
    }
}

/* --- API для инструкций VM -------------------------------------------- */
static inline bool id_ok(uint8_t id) { return id < MAX_TIMERS; }

void ton_set(uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &ton_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_ton(t);   /* сразу обновим — чтобы op_ton мог тут же получить Q */
}

bool ton_Q(uint8_t id) {
    if (!id_ok(id)) return false;
    return ton_timers[id].output;
}

void tof_set(uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &tof_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_tof(t);
}

bool tof_Q(uint8_t id) {
    if (!id_ok(id)) return false;
    return tof_timers[id].output;
}

void tp_set(uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &tp_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_tp(t);
}

bool tp_Q(uint8_t id) {
    if (!id_ok(id)) return false;
    return tp_timers[id].output;
}