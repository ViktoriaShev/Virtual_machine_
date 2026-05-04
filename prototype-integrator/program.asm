; test_alu.asm — ALU / logic / comparison tests for VM32
; Syntax: opcode A B C
; A, B — registers (R<n>)
; C    — register (R<n>) or immediate (#n)

; reg0 = 5, reg1 = 3 (initialized in VM main)
; r2 := 1; r3 := 0; r4 := failure_count

; --- init constants ---
div R2 R0 R0      ; r2 = 5 / 5 = 1
sub R3 R2 R2      ; r3 = 1 - 1 = 0
sub R4 R2 R2      ; r4 = 0  (failure_count)

; useful temp: r10 := 5 - 3 = 2
sub R10 R0 R1

; -----------------------
; Test 1: ADD (5 + 3 = 8)
; -----------------------
add R5 R0 R1
add R6 R0 R1
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 2: SUB (5 - 3 = 2)
; -----------------------
sub R5 R0 R1
sub R6 R1 R2
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 3: MUL (5 * 3 = 15)
; -----------------------
mul R5 R0 R1
mul R6 R0 R1
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 4: DIV normal (5 / 3 = 1)
; -----------------------
div R5 R0 R1
div R6 R0 R1
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 5: DIV by zero → 0
; -----------------------
div R5 R0 R3
sub R6 R2 R2
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 6: MOD (5 % 3 = 2)
; -----------------------
mod R5 R0 R1
sub R6 R0 R1
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 7: AND (5 & 3 = 1)
; -----------------------
and R5 R0 R1
div R6 R0 R0
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 8: OR (5 | 3 = 7)
; -----------------------
or  R5 R0 R1
add R11 R0 R1
sub R6 R11 R2
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 9: XOR (5 ^ 3 = 6)
; -----------------------
xor R5 R0 R1
sub R6 R11 R10
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 10: GT (5 > 3)
; -----------------------
gt  R5 R0 R1
div R6 R0 R0
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 11: LT (3 < 5)
; -----------------------
lt  R5 R1 R0
div R6 R0 R0
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; Test 12: EQ (5 == 5)
; -----------------------
eq  R5 R0 R0
div R6 R0 R0
eq  R7 R5 R6
sub R8 R2 R7
add R4 R4 R8

; -----------------------
; End: exit with failure_count
; -----------------------
exit R4
