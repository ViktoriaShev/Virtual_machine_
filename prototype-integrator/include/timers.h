#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_TIMERS 16

/* Forward declaration */
typedef struct vm_state vm_state_t;

/* IEC Timer structure */
typedef struct {
    bool enabled;
    bool input;
    bool output;
    bool prev_input;
    bool timing;
    struct timespec start;
    uint32_t preset_ms;
    uint32_t ET;  /* Elapsed Time */
} IEC_Timer;

/* Timer array initialization */
void timers_init(vm_state_t *vm);

/* Update all timers (called each VM cycle) */
void update_all_timers(vm_state_t *vm);

/* TON - On-Delay Timer */
void ton_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt);
bool ton_Q(vm_state_t *vm, uint8_t id);

/* TOF - Off-Delay Timer */
void tof_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt);
bool tof_Q(vm_state_t *vm, uint8_t id);

/* TP - Pulse Timer */
void tp_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt);
bool tp_Q(vm_state_t *vm, uint8_t id);

/* TONR - Retentive On-Delay Timer */
void tonr_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt);
bool tonr_Q(vm_state_t *vm, uint8_t id);

/* TOFR - Retentive Off-Delay Timer */
void tofr_set(vm_state_t *vm, uint8_t id, bool in, uint32_t pt);
bool tofr_Q(vm_state_t *vm, uint8_t id);

#endif /* TIMERS_H */