#define _POSIX_C_SOURCE 199309L
#include "timers.h"
#include <string.h>

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
            clock_gettime(CLOCK_MONOTONIC, &t->start);
            t->timing = true;
        }
        if (t->timing) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
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
            clock_gettime(CLOCK_MONOTONIC, &t->start);
            t->timing = true;
        }

        if (t->timing) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
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
        clock_gettime(CLOCK_MONOTONIC, &t->start);
        t->timing = true;
        t->output = true;
    }

    if (t->timing) {
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
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
            clock_gettime(CLOCK_MONOTONIC, &t->start);
            t->timing = true;
        }
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        if (t->timing) {
            uint32_t add = ms_diff(&t->start, &now);
            t->ET += add;
            clock_gettime(CLOCK_MONOTONIC, &t->start);
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
        clock_gettime(CLOCK_MONOTONIC, &t->start);
        t->timing = true;
    }

    if (t->timing) {
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        uint32_t add = ms_diff(&t->start, &now);
        t->ET += add;
        clock_gettime(CLOCK_MONOTONIC, &t->start);

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

#define SETTER(arr) \
void name(uint8_t id, bool in, uint32_t pt){ \
    if (id >= MAX_TIMERS) return; \
    arr[id].input = in; \
    arr[id].preset_ms = pt; \
    arr[id].enabled = true; \
}
