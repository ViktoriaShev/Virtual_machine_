#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "../include/vm32.h"

/* tiny test harness */
static int tests = 0;
static int fails = 0;
#define REPORT(name, cond) \
    do { tests++; if (cond) printf("PASS: %s\n", name); else { printf("FAIL: %s (line %d)\n", name, __LINE__); fails++; } } while(0)

/* helper: construct instruction word
   format: [ opcode:7 ][ A:8 ][ B:8 ][ IMM:1 ][ C:8 ] */
static inline uint32_t make_instr(uint8_t opcode, uint8_t A, uint8_t B, bool imm, uint8_t C) {
    return ((uint32_t)opcode << 25) |
           ((uint32_t)A << 17) |
           ((uint32_t)B << 9) |
           ((uint32_t)(imm ? 1 : 0) << 8) |
           ((uint32_t)C & 0xFF);
}

/* ---------- Позитивные тесты (как было) ---------- */

static void test_write_const_and_len(void) {
    memset(mem, 0, MEM_BYTES);
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;

    uint32_t dest = 0x1000;
    uint8_t ra = 1;
    reg[ra] = dest;

    uint32_t instr = make_instr(0, ra, 0, false, 0);
    op_write_const(instr, "hello");

    REPORT("write_const wrote string", strcmp((char*)(mem + dest), "hello") == 0);
    REPORT("null terminator present", mem[dest + 5] == 0);

    uint8_t rb = 2, ra_len = 3;
    reg[rb] = dest;
    reg[ra_len] = 0;
    uint32_t instr_len = make_instr(0, ra_len, rb, false, 0);
    op_len(instr_len);
    REPORT("op_len returns 5", reg[ra_len] == 5);
}

static void test_write_string_with_maxlen(void) {
    memset(mem, 0, MEM_BYTES);
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;

    const char *src = "abcdefghij";
    uint32_t srcAddr = 0x2000;
    memcpy(mem + srcAddr, src, strlen(src)+1);

    uint32_t destAddr = 0x3000;
    uint8_t ra = 4, rb = 5;
    reg[ra] = destAddr;
    reg[rb] = srcAddr;

    uint8_t maxLen = 4;
    uint32_t instr = make_instr(0, ra, rb, false, maxLen);
    op_write_string(instr);

    REPORT("write_string truncated to maxLen", memcmp(mem + destAddr, "abcd", 4) == 0);
    REPORT("write_string null after truncated part", mem[destAddr + 4] == '\0');

    uint8_t ra_len = 6, rb_addr = 5;
    reg[rb_addr] = destAddr;   /* <-- важно: указываем на буфер куда записалось */
    reg[ra_len] = 0;
    uint32_t instr_len = make_instr(0, ra_len, rb_addr, false, 0);
    op_len(instr_len);
    REPORT("len after write_string == 4", reg[ra_len] == 4);

}

static void test_concat_basic(void) {
    memset(mem, 0, MEM_BYTES);
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;

    uint32_t dest = 0x4000, src = 0x5000;
    memcpy(mem + dest, "hello", 6);
    memcpy(mem + src, "WORLD", 6);

    uint8_t rb = 7, rc = 8;
    reg[rb] = dest;
    reg[rc] = src;

    uint32_t instr = make_instr(0, 0, rb, false, rc);
    op_concat(instr);

    REPORT("concat result matches", strcmp((char*)(mem + dest), "helloWORLD") == 0);
    REPORT("concat null terminator present", mem[dest + strlen("helloWORLD")] == '\0');
}

static void test_left_right_mid(void) {
    memset(mem, 0, MEM_BYTES);
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;

    uint32_t addr = 0x6000;
    memcpy(mem + addr, "abcdefg", 8);

    uint8_t rb = 9;
    reg[rb] = addr;
    uint32_t instr_left = make_instr(0, 0, rb, true, 3);
    op_left(instr_left);
    REPORT("left -> \"abc\"", strcmp((char*)(mem + addr), "abc") == 0);

    memcpy(mem + addr, "abcdefg", 8);
    reg[rb] = addr;
    uint32_t instr_right = make_instr(0, 0, rb, true, 3);
    op_right(instr_right);
    REPORT("right -> last 3 \"efg\"", strcmp((char*)(mem + addr), "efg") == 0);

    memcpy(mem + addr, "abcdefg", 8);
    reg[rb] = addr;
    uint32_t instr_mid = make_instr(0, 0, rb, true, 2);
    op_mid(instr_mid);
    REPORT("mid start=2 -> \"cdefg\"", strcmp((char*)(mem + addr), "cdefg") == 0);
}

static void test_insert_delete_replace(void) {
    memset(mem, 0, MEM_BYTES);
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;

    uint32_t dest = 0x7000;
    uint32_t ins = 0x7100;
    memcpy(mem + dest, "hello", 6);
    memcpy(mem + ins, "123", 4);

    uint8_t ra = 10, rb = 11;
    reg[ra] = dest;
    reg[rb] = ins;
    uint32_t instr_insert = make_instr(0, ra, rb, true, 2);
    op_insert(instr_insert);
    REPORT("insert in middle -> he123llo", strcmp((char*)(mem + dest), "he123llo") == 0);

    uint32_t small_dest = (uint32_t)(MEM_BYTES - 4);
    uint32_t ins2 = 0x7200;
    memcpy(mem + small_dest, "XY", 3);
    memcpy(mem + ins2, "LONGSTRING", 11);

    uint8_t ra2 = 12, rb2 = 13;
    reg[ra2] = small_dest;
    reg[rb2] = ins2;
    uint32_t instr_insert2 = make_instr(0, ra2, rb2, true, 1);
    op_insert(instr_insert2);

    size_t rem = MEM_BYTES - small_dest;
    bool nul_inside = false;
    for (size_t i = 0; i < rem; ++i) {
        if (mem[small_dest + i] == '\0') { nul_inside = true; break; }
    }
    REPORT("insert near end leaves null terminator inside memory", nul_inside);

    uint32_t del_addr = 0x7300;
    memcpy(mem + del_addr, "ABCDEFG", 8);
    uint8_t ra_del = 14, rb_pos = 15;
    reg[ra_del] = del_addr;
    reg[rb_pos] = 3;
    uint32_t instr_del = make_instr(0, ra_del, rb_pos, true, 100);
    op_delete(instr_del);
    REPORT("delete clamps larger-than-tail -> \"ABC\"", strcmp((char*)(mem + del_addr), "ABC") == 0);

    uint32_t rep_dest = 0x7400;
    uint32_t rep_src = 0x7500;
    memcpy(mem + rep_dest, "foobarbaz", 10);
    memcpy(mem + rep_src, "XYZ", 4);

    uint8_t ra_rep = 16, rb_rep = 17;
    reg[ra_rep] = rep_dest;
    reg[rb_rep] = rep_src;
    uint32_t instr_rep = make_instr(0, ra_rep, rb_rep, true, 3);
    op_replace(instr_rep);
    bool prefix_ok = (memcmp(mem + rep_dest, "foo", 3) == 0);
    bool inserted_ok = (memcmp(mem + rep_dest + 3, "XYZ", 3) == 0);
    REPORT("replace inserted at pos with expected prefix and inserted bytes", prefix_ok && inserted_ok);
}

/* ---------- Негативные тесты (граничные / некорректные адреса) ---------- */

static void test_negative_cases(void) {
    memset(mem, 0, MEM_BYTES);
    for (int i=0;i<REG_COUNT;i++) reg[i]=0;

    /* sentinel bytes to detect accidental corruption */
    mem[0] = 0x55;
    mem[1] = 0x55;
    mem[100] = 0x55;

    /* 1) op_write_const with dest >= MEM_BYTES should do nothing (safe early return) */
    uint8_t ra_bad = 20;
    reg[ra_bad] = (uint32_t)MEM_BYTES; /* invalid */
    uint32_t instr_wc_bad = make_instr(0, ra_bad, 0, false, 0);
    op_write_const(instr_wc_bad, "bad");
    REPORT("op_write_const with dest>=MEM_BYTES does not corrupt sentinel[0]", mem[0] == 0x55 && mem[1] == 0x55);

    /* 2) op_write_string with src >= MEM_BYTES should not write */
    uint32_t dest_ok = 0x8000;
    uint8_t ra_ok = 21, rb_src_bad = 22;
    reg[ra_ok] = dest_ok;
    reg[rb_src_bad] = (uint32_t)MEM_BYTES; /* invalid src */
    uint32_t instr_ws_bad = make_instr(0, ra_ok, rb_src_bad, false, 10);
    op_write_string(instr_ws_bad);
    REPORT("op_write_string with src>=MEM_BYTES does not write (sentinel intact)", mem[0] == 0x55 && mem[100] == 0x55);

    /* 3) op_concat with dest >= MEM_BYTES or src >= MEM_BYTES should be noop */
    uint8_t rb_bad = 23, rc_bad = 24;
    reg[rb_bad] = (uint32_t)MEM_BYTES;
    reg[rc_bad] = 0x9000;
    uint32_t instr_concat_bad = make_instr(0, 0, rb_bad, false, rc_bad);
    op_concat(instr_concat_bad);
    REPORT("op_concat with dest>=MEM_BYTES safe (sentinel intact)", mem[1] == 0x55 && mem[100] == 0x55);

    /* 4) op_len with addr >= MEM_BYTES should set RA to 0 */
    uint8_t ra_len = 25, rb_len_bad = 26;
    reg[rb_len_bad] = (uint32_t)MEM_BYTES;
    reg[ra_len] = 12345;
    uint32_t instr_len_bad = make_instr(0, ra_len, rb_len_bad, false, 0);
    op_len(instr_len_bad);
    REPORT("op_len(addr>=MEM_BYTES) yields 0", reg[ra_len] == 0);

    /* 5) op_insert with dest invalid or insAddr invalid should not corrupt */
    uint8_t ra_ins_bad = 27, rb_ins_bad = 28;
    reg[ra_ins_bad] = (uint32_t)MEM_BYTES; /* dest invalid */
    reg[rb_ins_bad] = 0xA000;
    uint32_t instr_ins_bad = make_instr(0, ra_ins_bad, rb_ins_bad, true, 1);
    op_insert(instr_ins_bad);
    REPORT("op_insert with dest>=MEM_BYTES safe", mem[0] == 0x55 && mem[1] == 0x55);

    /* 6) op_delete with addr >= MEM_BYTES is noop */
    uint8_t ra_del_bad = 29, rb_pos = 30;
    reg[ra_del_bad] = (uint32_t)MEM_BYTES;
    reg[rb_pos] = 1;
    uint32_t instr_del_bad = make_instr(0, ra_del_bad, rb_pos, true, 2);
    op_delete(instr_del_bad);
    REPORT("op_delete with addr>=MEM_BYTES safe", mem[0] == 0x55 && mem[100] == 0x55);

    /* 7) op_replace with repAddr invalid should be noop */
    uint32_t rep_dest = 0xB000;
    uint8_t ra_rep = 31, rb_rep_bad = 32;
    reg[ra_rep] = rep_dest;
    reg[rb_rep_bad] = (uint32_t)MEM_BYTES;
    uint32_t instr_rep_bad = make_instr(0, ra_rep, rb_rep_bad, true, 0);
    op_replace(instr_rep_bad);
    REPORT("op_replace with repAddr>=MEM_BYTES safe", mem[1] == 0x55 && mem[100] == 0x55);

    /* 8) Edge: op_write_const to MEM_BYTES-1 should only write a terminating zero at last byte (no overflow) */
    uint32_t last_addr = (uint32_t)MEM_BYTES - 1;
    uint8_t ra_last = 33;
    reg[ra_last] = last_addr;
    uint32_t instr_last = make_instr(0, ra_last, 0, false, 0);
    op_write_const(instr_last, "xyz");
    REPORT("op_write_const at MEM_BYTES-1 writes only null terminator in-bounds", mem[last_addr] == '\0');
}

/* main runner */
int main(void) {
    mem = (uint8_t *)calloc((size_t)MEM_BYTES, 1);
    if (!mem) { perror("calloc mem"); return 2; }

    printf("=== string ops unit tests ===\n");
    test_write_const_and_len();
    test_write_string_with_maxlen();
    test_concat_basic();
    test_left_right_mid();
    test_insert_delete_replace();
    test_negative_cases();

    printf("=========================================\n");
    if (fails == 0) {
        printf("ALL STRING TESTS PASSED (%d)\n", tests);
        free(mem);
        return 0;
    } else {
        printf("FAILED: %d / %d\n", fails, tests);
        free(mem);
        return 1;
    }
}

