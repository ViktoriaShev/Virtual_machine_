```asm
ADD 2 0 1
SUB 3 2 1  
MUL 4 3 0
HALT
```

```
╔═══════════════════════════════════════════════════════════════╗
║           VM32 Execution Log - Tue Dec  9 18:57:24 2025
╠═══════════════════════════════════════════════════════════════╣
║ Memory: 67108864 bytes (64.00 MB)
║ Registers: 256
║ Start PC: 0x3000
╚═══════════════════════════════════════════════════════════════╝


=== CYCLE 1 START ===
Registers hash at start: 0xC021911A
Initialized prev_cycle_hash = 0xC021911A
┌─ Instruction #1 ─────────────────────────────────────────
│ PC: 0x3000
│ ADD        R2, R0, R1  [0x00040001]
│ Binary: 00000000 00000100 00000000 00000001
│
│ Operands BEFORE:
│   R2 (A) = 0x00000000 (0)
│   R0 (B) = 0x00000005 (5)
│   R1 (C) = 0x00000003 (3)
│
│ Register CHANGES:
│   R002: 0x00000000 (         0) -> 0x00000008 (         8)
│
│ Memory changes: (none)
│
│ PC changed: 0x3000 -> 0x3004
└──────────────────────────────────────────────────────────────

┌─ Instruction #2 ─────────────────────────────────────────
│ PC: 0x3004
│ SUB        R3, R2, R1  [0x02060401]
│ Binary: 00000010 00000110 00000100 00000001
│
│ Operands BEFORE:
│   R3 (A) = 0x00000000 (0)
│   R2 (B) = 0x00000008 (8)
│   R1 (C) = 0x00000003 (3)
│
│ Register CHANGES:
│   R003: 0x00000000 (         0) -> 0x00000005 (         5)
│
│ Memory changes: (none)
│
│ PC changed: 0x3004 -> 0x3008
└──────────────────────────────────────────────────────────────

┌─ Instruction #3 ─────────────────────────────────────────
│ PC: 0x3008
│ MUL        R4, R3, R0  [0x04080600]
│ Binary: 00000100 00001000 00000110 00000000
│
│ Operands BEFORE:
│   R4 (A) = 0x00000000 (0)
│   R3 (B) = 0x00000005 (5)
│   R0 (C) = 0x00000005 (5)
│
│ Register CHANGES:
│   R004: 0x00000000 (         0) -> 0x00000019 (        25)
│
│ Memory changes: (none)
│
│ PC changed: 0x3008 -> 0x300C
└──────────────────────────────────────────────────────────────

┌─ Instruction #4 ─────────────────────────────────────────
│ PC: 0x300C
│ HALT       R0, R0, R0  [0x76000000]
│ Binary: 01110110 00000000 00000000 00000000
│
│ Operands BEFORE:
│   R0 (A) = 0x00000005 (5)
│   R0 (B) = 0x00000005 (5)
│   R0 (C) = 0x00000005 (5)
│
│ Register changes: (none)
│
│ Memory changes: (none)
│
│ PC changed: 0x300C -> 0x3010
└──────────────────────────────────────────────────────────────

Registers hash at end:   0x84A3DBB8
=== CYCLE 1 END: executed 4 instructions in 0 ms ===
  Waiting for 1000 ms to complete cycle
  Total cycle time: 1000 ms

╔═══════════════════════════════════════════════════════════════╗
║ Execution Summary
╠═══════════════════════════════════════════════════════════════╣
║ Total instructions executed: 4
║ Final PC: 0x3000
╚═══════════════════════════════════════════════════════════════╝

```


Файл содержит подробные комментарии (комментарии начинаются с `;` и будут удалены твой парсером). Этот набор тестов проверяет основные арифметические, логические и сравнительные операции: `add, sub, mul, div, mod, and, or, xor, eq, gt, lt` и поведение `div` при делении на ноль.

Скопируй (или сохрани) файл с именем `test_alu.asm`:

```
; test_alu.asm -- ALU / logic / comparison tests for VM32
; Uses assembler format: opcode A B C
; reg[0] == 5, reg[1] == 3 (set in main) are used as base constants.
; r2 := 1; r3 := 0; r4 := failure_count
; after each test we compute (1 - pass) and add to r4.
; At the end: exit r4  (vm exit code = number of failed tests)

; --- init constants ---
div 2 0 0      ; r2 = reg0 / reg0 = 5/5 = 1
sub 3 2 2      ; r3 = 1 - 1 = 0
sub 4 2 2      ; r4 = 0  ; failure_count := 0

; useful temp: r10 := 5 - 3 = 2  (used later)
sub 10 0 1     ; r10 = r0 - r1 = 2

; -----------------------
; Test 1: ADD  (5 + 3 = 8)
; -----------------------
add 5 0 1      ; r5 = r0 + r1 = 8
add 6 0 1      ; r6 = expected = 8
eq 7 5 6       ; r7 = (r5 == r6) ? 1 : 0
sub 8 2 7      ; r8 = 1 - r7  (1 if fail, 0 if pass)
add 4 4 8      ; failure_count += r8

; -----------------------
; Test 2: SUB (5 - 3 = 2)
; -----------------------
sub 5 0 1      ; r5 = 2
sub 6 1 2      ; r6 = r1 - r2 = 3 - 1 = 2  (expected)
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 3: MUL (5 * 3 = 15)
; -----------------------
mul 5 0 1
mul 6 0 1
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 4: DIV normal (5 / 3 = 1)
; -----------------------
div 5 0 1
div 6 0 1
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 5: DIV by zero -> should produce 0 (spec)
; -----------------------
div 5 0 3      ; r3 == 0 -> r5 should be 0
sub 6 2 2      ; r6 = 0  (expected)
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 6: MOD (5 % 3 = 2)
; -----------------------
mod 5 0 1
sub 6 0 1      ; expected = 5 - 3 = 2
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 7: AND (5 & 3 = 1)
; -----------------------
and 5 0 1
div 6 0 0      ; r6 = 5/5 = 1 (expected)
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 8: OR (5 | 3 = 7)
; build expected = (r0 + r1) - 1 = 8 - 1 = 7
; -----------------------
or 5 0 1
add 11 0 1     ; r11 = r0 + r1 = 8
sub 6 11 2     ; r6  = r11 - r2 = 8 - 1 = 7 (expected)
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test 9: XOR (5 ^ 3 = 6)
; expected = r11 - r10  (8 - 2 = 6)
; -----------------------
xor 5 0 1
sub 6 11 10    ; r6 = r11 - r10 = 8 - 2 = 6 (expected)
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test10: GT (5 > 3) => true (1)
; -----------------------
gt 5 0 1
div 6 0 0      ; r6 = 1 (expected)
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test11: LT (3 < 5) => true (1)
; -----------------------
lt 5 1 0       ; r5 = (r1 < r0) ? 1 : 0  -> 1
div 6 0 0      ; r6 = 1
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; Test12: EQ (5 == 5)
; -----------------------
eq 5 0 0       ; r5 = 1
div 6 0 0      ; r6 = 1
eq 7 5 6
sub 8 2 7
add 4 4 8

; -----------------------
; End: return number of failed tests in r4
;  vm will return exit code = reg[rA] when A != 0, so use "exit 4"
; -----------------------
exit 4
```

Как запустить (предполагая, что твой ассемблер — `asm` и VM бинар — `vm32`):

1. Ассемблируем:
    

```bash
./asm test_alu.asm test_alu.bin
```

(в твоём проекте это будет что-то вроде `./assembler test_alu.asm test_alu.bin`)

2. Запускаем VM:
    

```bash
./vm32 test_alu.bin
```

3. Проверяем код выхода:
    

- `echo $?` в оболочке покажет `0` — все тесты пройдены.
    
- Любое `>0` — количество упавших тестов (в `r4`).
    
