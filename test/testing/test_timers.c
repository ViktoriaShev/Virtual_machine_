// testing/test_timers.c
// Unit-tests for timers (TON/TOF/TP)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "timers.h"   // должен объявлять ton_set/tof_set/tp_set, *_Q, update_all_timers, timers_init
#include "vm32.h"     // для объявления time_ms (extern)

/* Define the global time_ms used by timers.c (vm32 normally defines it) */
uint64_t time_ms = 0;

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void pass(const char *msg) {
    printf("PASS: %s\n", msg);
}

/* TON basic behaviour:
 * - IN rising starts timing
 * - Q becomes 1 when elapsed >= preset
 * - resetting IN -> Q == 0
 */
static void test_ton_basic(void) {
    timers_init();
    time_ms = 0;

    const uint8_t id = 2;
    const uint32_t pt = 200; // ms

    // ensure clear initial state
    ton_set(id, false, pt);
    if (ton_Q(id)) fail("TON initial Q should be 0");

    // rising edge -> start timing
    ton_set(id, true, pt); // this calls update_ton immediately
    if (ton_Q(id)) fail("TON Q must be 0 immediately after start");

    // just before preset
    time_ms = 199;
    update_all_timers();
    if (ton_Q(id)) fail("TON Q must still be 0 at 199ms");

    // exactly at preset
    time_ms = 200;
    update_all_timers();
    if (!ton_Q(id)) fail("TON Q must be 1 at 200ms");

    // keep input = true -> Q remains 1
    time_ms = 300;
    update_all_timers();
    if (!ton_Q(id)) fail("TON Q should stay 1 while input = true after PT");

    // dropping input resets Q
    ton_set(id, false, pt);
    if (ton_Q(id)) fail("TON Q must be 0 after input cleared");

    pass("TON basic behaviour");
}

/* TOF basic behaviour:
 * - IN=1 => Q=1
 * - falling edge starts off-delay; Q stays 1 until elapsed >= preset, then becomes 0
 */
static void test_tof_basic(void) {
    timers_init();
    time_ms = 0;

    const uint8_t id = 3;
    const uint32_t pt = 300; // ms

    // set IN high => Q should be 1
    tof_set(id, true, pt);
    if (!tof_Q(id)) fail("TOF Q must be 1 while IN=1");

    // falling edge starts off-delay
    tof_set(id, false, pt);
    // right after falling, before PT elapsed -> Q still 1
    time_ms = 299;
    update_all_timers();
    if (!tof_Q(id)) fail("TOF Q should still be 1 at 299ms after falling");

    // after PT -> Q becomes 0
    time_ms = 300;
    update_all_timers();
    if (tof_Q(id)) fail("TOF Q should be 0 at 300ms after falling");

    pass("TOF basic behaviour");
}

/* TP basic behaviour (pulse on rising):
 * - rising edge -> Q=1 immediately, stays 1 for PT ms, then 0
 * - repeated rising during pulse should not extend the pulse (test nominal)
 */
static void test_tp_basic(void) {
    timers_init();
    time_ms = 0;

    const uint8_t id = 4;
    const uint32_t pt = 200;

    // ensure off initially
    tp_set(id, false, pt);
    if (tp_Q(id)) fail("TP initial Q should be 0");

    // rising -> pulse starts
    tp_set(id, true, pt);
    if (!tp_Q(id)) fail("TP Q must be 1 immediately after rising");

    // mid-pulse
    time_ms = 100;
    update_all_timers();
    if (!tp_Q(id)) fail("TP Q should be 1 at 100ms");

    // another rising in the middle - should not restart pulse (should still end at ~200)
    tp_set(id, true, pt); // re-assert
    // still in pulse
    time_ms = 199;
    update_all_timers();
    if (!tp_Q(id)) fail("TP Q should still be 1 at 199ms even after re-assert");

    // at cutoff
    time_ms = 200;
    update_all_timers();
    if (tp_Q(id)) fail("TP Q should be 0 at 200ms (pulse ended)");

    pass("TP basic behaviour");
}

/* Edge-case: preset==0 behaviour (should yield no timing / immediate false for TON and TP)
 * For TON/TP the code treats preset==0 as disabled -> output false, ET=0.
 */
static void test_preset_zero(void) {
    timers_init();
    time_ms = 0;

    const uint8_t id = 5;
    const uint32_t pt = 0;

    ton_set(id, true, pt);
    if (ton_Q(id)) fail("TON with PT=0 must yield Q=0 (disabled)");

    tp_set(id, true, pt);
    if (tp_Q(id)) fail("TP with PT=0 must yield Q=0 (disabled)");

    // TOF with PT=0 -> treated as disabled (in code), output false
    tof_set(id, true, pt);
    if (tof_Q(id)) fail("TOF with PT=0 must yield Q=0 (disabled)");

    pass("Preset == 0 behaviour");
}

int main(void) {
    printf("=== timers unit tests ===\n");

    test_ton_basic();
    test_tof_basic();
    test_tp_basic();
    test_preset_zero();

    printf("ALL TESTS PASSED\n");
    return 0;
}
