// testing/test_timers.c
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "../include/timers.h"
#include "../include/vm32.h"

// Простая тестовая harness
static int tests_run = 0;
static int tests_failed = 0;
uint64_t time_ms = 0;

static void report(const char *name, bool ok) {
    tests_run++;
    if (ok) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        tests_failed++;
    }
}

/* Advance simulated VM time and update timers */
static void advance_ms(uint64_t ms) {
    time_ms += ms;
    update_all_timers();
}

/* Helpers */
static void reset_env(void) {
    timers_init();
    time_ms = 0;
}

/* === Tests === */

static void test_ton_edges(void) {
    reset_env();
    const uint8_t id = 5;
    const uint32_t pt = 100; // ms

    ton_set(id, false, pt);
    report("TON edges: initial Q==0", ton_Q(id) == false);

    ton_set(id, true, pt); // rising -> start
    report("TON edges: just after start Q==0", ton_Q(id) == false);

    advance_ms(pt - 1);
    report("TON edges: at pt-1 Q==0", ton_Q(id) == false);

    advance_ms(1);
    report("TON edges: at pt Q==1", ton_Q(id) == true);

    advance_ms(10);
    report("TON edges: after pt Q stays 1", ton_Q(id) == true);
}

static void test_ton_pt_neighbors(void) {
    reset_env();
    const uint8_t id = 9;
    const uint32_t pt = 50;

    // Start at t=0
    ton_set(id, true, pt);
    advance_ms(pt - 1);
    report("TON neighbor: pt-1 false", ton_Q(id) == false);

    // step to pt
    advance_ms(1);
    report("TON neighbor: pt true", ton_Q(id) == true);

    // now change PT to longer and restart
    ton_set(id, false, pt);
    ton_set(id, true, pt + 1);
    advance_ms(pt);         // should still be false (need pt+1)
    report("TON neighbor: new pt pt false", ton_Q(id) == false);
    advance_ms(1);
    report("TON neighbor: new pt pt+1 true", ton_Q(id) == true);
}

static void test_ton_parallel(void) {
    reset_env();
    const uint8_t id1 = 1, id2 = 2;
    const uint32_t pt1 = 50, pt2 = 150;

    ton_set(id1, true, pt1);
    ton_set(id2, true, pt2);

    advance_ms(60);
    report("TON parallel: id1 should be 1 at 60ms", ton_Q(id1) == true);
    report("TON parallel: id2 should be 0 at 60ms", ton_Q(id2) == false);

    advance_ms(100); // now t = 160
    report("TON parallel: id2 should be 1 at 160ms", ton_Q(id2) == true);
}

static void test_ton_reenable(void) {
    reset_env();
    const uint8_t id = 7;
    const uint32_t pt = 80;

    ton_set(id, true, pt);
    advance_ms(90);
    report("TON reenable: after first PT Q==1", ton_Q(id) == true);

    ton_set(id, false, pt);
    report("TON reenable: after IN=false Q==0", ton_Q(id) == false);

    ton_set(id, true, pt);
    advance_ms(pt - 1);
    report("TON reenable: after reenable at pt-1 Q==0", ton_Q(id) == false);
    advance_ms(1);
    report("TON reenable: after reenable at pt Q==1", ton_Q(id) == true);
}

static void test_tof_basic(void) {
    reset_env();
    const uint8_t id = 3;
    const uint32_t pt = 120;

    tof_set(id, true, pt);
    report("TOF basic: IN=1 -> Q==1", tof_Q(id) == true);

    tof_set(id, false, pt); // falling
    report("TOF basic: just after fall Q still 1", tof_Q(id) == true);

    advance_ms(pt - 1);
    report("TOF basic: at pt-1 Q==1", tof_Q(id) == true);

    advance_ms(1);
    report("TOF basic: at pt Q==0", tof_Q(id) == false);
}

static void test_tp_pulse(void) {
    reset_env();
    const uint8_t id = 4;
    const uint32_t pt = 200;

    tp_set(id, true, pt);
    report("TP pulse: immediately after rising Q==1", tp_Q(id) == true);

    advance_ms(pt - 1);
    report("TP pulse: at pt-1 Q==1", tp_Q(id) == true);

    advance_ms(1);
    report("TP pulse: at pt Q==0", tp_Q(id) == false);

    // new pulse after previous finished
    tp_set(id, false, pt);
    tp_set(id, true, pt);
    report("TP pulse: second pulse start Q==1", tp_Q(id) == true);
}

static void test_preset_zero_behavior(void) {
    reset_env();
    const uint8_t id = 10;
    const uint32_t pt = 0;

    // code treats preset==0 as disabled => output false and ET==0
    ton_set(id, true, pt);
    report("Preset==0 TON: Q==false", ton_Q(id) == false);

    tof_set(id, true, pt);
    report("Preset==0 TOF: Q==false", tof_Q(id) == false);

    tp_set(id, true, pt);
    report("Preset==0 TP: Q==false", tp_Q(id) == false);
}

static void test_id_bounds(void) {
    reset_env();
#ifdef MAX_TIMERS
    uint8_t last = (uint8_t)(MAX_TIMERS - 1);
    ton_set(last, true, 10);
    // just ensure calling does not crash and returns a bool
    report("ID bounds: last id accessible", ton_Q(last) == false || ton_Q(last) == true);

    uint8_t bad = (uint8_t)(MAX_TIMERS + 1);
    bool ok = (ton_Q(bad) == false);
    report("ID bounds: out-of-range id returns false Q", ok);
#else
    report("ID bounds: MAX_TIMERS not defined", false);
#endif
}

static void test_multiple_sets_during_timing(void) {
    reset_env();
    const uint8_t id = 6;
    const uint32_t pt = 100;

    ton_set(id, true, pt);
    advance_ms(40);
    ton_set(id, true, pt); // re-set with same in -> should not reset timing
    advance_ms(59);
    report("Multiple sets: just before pt Q==0", ton_Q(id) == false);
    advance_ms(1);
    report("Multiple sets: after pt Q==1", ton_Q(id) == true);
}

static void test_many_timers_independence(void) {
    reset_env();
    const int n = 12;
    for (int i = 0; i < n; ++i) {
        ton_set((uint8_t)i, true, 20 + i * 10); // staggered presets
    }
    advance_ms(25);
    bool ok = true;
    for (int i = 0; i < n; ++i) {
        bool q = ton_Q((uint8_t)i);
        if ( (20 + i*10) <= 25 ) {
            ok &= q == true;
        } else {
            ok &= q == false;
        }
    }
    report("Many timers independence (TON)", ok);
}

static void test_stress_toggle_pattern(void) {
    reset_env();
    const uint8_t id = 11;
    const uint32_t pt = 150;
    // Deterministic pseudo-random toggle pattern (LCG)
    uint32_t seed = 0xA5A5A5A5;
    ton_set(id, false, pt);
    for (int step = 0; step < 500; ++step) {
        seed = (1103515245u * seed + 12345u);
        bool in = (seed >> 16) & 1;
        ton_set(id, in, pt);
        // advance 1..5 ms pseudo-random
        uint32_t adv = 1 + ((seed >> 8) & 0x7) ;
        advance_ms(adv);
    }
    // final sanity: Q must be boolean (no crash) — just check call works
    report("Stress toggle pattern completed (no crash)", ton_Q(id) == false || ton_Q(id) == true);
}

static void test_integration_multicycle(void) {
    reset_env();
    const uint8_t id = 8;
    const uint32_t pt = 1200; // > 1000 ms to force cross-cycle behaviour

    ton_set(id, true, pt);
    advance_ms(1000);
    report("Integration multicyle: after 1000ms still false", ton_Q(id) == false);
    advance_ms(200);
    report("Integration multicyle: after 1200ms true", ton_Q(id) == true);
}

/* Run all tests */
int main(void) {
    printf("=== timers unit tests ===\n");

    test_ton_edges();
    test_ton_pt_neighbors();
    test_ton_parallel();
    test_ton_reenable();
    test_tof_basic();
    test_tp_pulse();
    test_preset_zero_behavior();
    test_id_bounds();
    test_multiple_sets_during_timing();
    test_many_timers_independence();
    test_stress_toggle_pattern();
    test_integration_multicycle();

    if (tests_failed == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d of %d\n", tests_failed, tests_run);
        return 1;
    }
}
