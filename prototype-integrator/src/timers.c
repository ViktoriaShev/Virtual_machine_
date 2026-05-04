#include "timers.h"
#include "main.h"
#include <string.h>

static inline uint32_t ms_diff(struct timespec *a, struct timespec *b) {
    return (uint32_t)((b->tv_sec - a->tv_sec) * 1000ULL +
                      (b->tv_nsec - a->tv_nsec) / 1000000ULL);
}

void timers_init(vm_state_t *vm) {
    memset(vm->ton_timers, 0, sizeof(vm->ton_timers));
    memset(vm->tof_timers, 0, sizeof(vm->tof_timers));
    memset(vm->tp_timers, 0, sizeof(vm->tp_timers));
    memset(vm->tonr_timers, 0, sizeof(vm->tonr_timers));
    memset(vm->tofr_timers, 0, sizeof(vm->tofr_timers));
}

/* --- IEC 61131-3 Logics ------------------------------------------------ */

static void update_ton(vm_state_t *vm, IEC_Timer *t) {
    if (!t->enabled || t->preset_ms == 0) {
        t->output = false;
        t->ET = 0;
        return;
    }

    bool rising = (t->input && !t->prev_input);

    if (t->input) {
        if (rising) {
            t->start.tv_sec = (time_t)(vm->time_ms / 1000ULL);
            t->start.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
            t->timing = true;
        }
        if (t->timing) {
            struct timespec now;
            now.tv_sec = (time_t)(vm->time_ms / 1000ULL);
            now.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
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

static void update_tof(vm_state_t *vm, IEC_Timer *t) {
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
            t->start.tv_sec = (time_t)(vm->time_ms / 1000ULL);
            t->start.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
            t->timing = true;
        }

        if (t->timing) {
            struct timespec now;
            now.tv_sec = (time_t)(vm->time_ms / 1000ULL);
            now.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
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

static void update_tp(vm_state_t *vm, IEC_Timer *t) {
    if (!t->enabled || t->preset_ms == 0) {
        t->output = false;
        t->ET = 0;
        return;
    }

    bool rising = (t->input && !t->prev_input);

    if (rising) {
        t->start.tv_sec = (time_t)(vm->time_ms / 1000ULL);
        t->start.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
        t->timing = true;
        t->output = true;
    }

    if (t->timing) {
        struct timespec now;
        now.tv_sec = (time_t)(vm->time_ms / 1000ULL);
        now.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
        t->ET = ms_diff(&t->start, &now);
        if (t->ET >= t->preset_ms) {
            t->output = false;
            t->timing = false;
        }
    }

    t->prev_input = t->input;
}

static void update_tonr(vm_state_t *vm, IEC_Timer *t) {
    if (!t->enabled) return;

    bool rising = (t->input && !t->prev_input);

    if (t->input) {
        if (rising) {
            t->start.tv_sec = (time_t)(vm->time_ms / 1000ULL);
            t->start.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
            t->timing = true;
        }
        struct timespec now;
        now.tv_sec = (time_t)(vm->time_ms / 1000ULL);
        now.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
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

static void update_tofr(vm_state_t *vm, IEC_Timer *t) {
    if (!t->enabled) return;

    bool falling = (!t->input && t->prev_input);

    if (falling) {
        t->start.tv_sec = (time_t)(vm->time_ms / 1000ULL);
        t->start.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
        t->timing = true;
    }

    if (t->timing) {
        struct timespec now;
        now.tv_sec = (time_t)(vm->time_ms / 1000ULL);
        now.tv_nsec = (long)((vm->time_ms % 1000ULL) * 1000000ULL);
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

void update_all_timers(vm_state_t *vm) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        update_ton(vm, &vm->ton_timers[i]);
        update_tof(vm, &vm->tof_timers[i]);
        update_tp(vm, &vm->tp_timers[i]);
        update_tonr(vm, &vm->tonr_timers[i]);
        update_tofr(vm, &vm->tofr_timers[i]);
    }
}

/* --- API для инструкций VM -------------------------------------------- */

static inline bool id_ok(uint8_t id) { 
    return id < MAX_TIMERS; 
}

void ton_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &vm->ton_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_ton(vm, t);
}

bool ton_Q(vm_state_t *vm, uint8_t id) {
    if (!id_ok(id)) return false;
    return vm->ton_timers[id].output;
}

void tof_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &vm->tof_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_tof(vm, t);
}

bool tof_Q(vm_state_t *vm, uint8_t id) {
    if (!id_ok(id)) return false;
    return vm->tof_timers[id].output;
}

void tp_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &vm->tp_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_tp(vm, t);
}

bool tp_Q(vm_state_t *vm, uint8_t id) {
    if (!id_ok(id)) return false;
    return vm->tp_timers[id].output;
}

void tonr_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &vm->tonr_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_tonr(vm, t);
}

bool tonr_Q(vm_state_t *vm, uint8_t id) {
    if (!id_ok(id)) return false;
    return vm->tonr_timers[id].output;
}

void tofr_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt) {
    if (!id_ok(id)) return;
    IEC_Timer *t = &vm->tofr_timers[id];
    t->input = in;
    t->preset_ms = pt;
    t->enabled = true;
    update_tofr(vm, t);
}

bool tofr_Q(vm_state_t *vm, uint8_t id) {
    if (!id_ok(id)) return false;
    return vm->tofr_timers[id].output;
}