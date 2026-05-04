; examples_all_ops.asm
; Примеры для всех op-кодов (двойные варианты, где поддерживается immediate #)
; Формат: mnemonic A, B, C
; A/B = регистры R<n>, C = R<n> или #<imm> если поддерживается

; -- подготовка: обнулим R0 (по умолчанию в vm.reg[0] = 5)
sub R0, R0, R0        ; R0 = R0 - R0 -> 0
; зададим пару опорных регистров
add R1, R0, #10       ; R1 = 0 + 10 -> 10
add R2, R0, #3        ; R2 = 3
add R3, R0, #7        ; R3 = 7

; ===== Арифметика =====
add R4, R1, R2        ; R4 = R1 + R2  (C as reg)
add R5, R1, #5        ; R5 = R1 + 5   (C immediate)

sub R6, R1, R3
sub R7, R1, #2

mul R8, R2, R3
mul R9, R2, #4

div R10, R1, R2
div R11, R1, #2

mod R12, R1, R2
mod R13, R1, #3

expt R14, R2, R3
expt R15, R2, #2

abs R16, R1           ; abs uses B, C ignored
sqrt R17, R1
ln R18, R1
log R19, R1
exp R20, R1
sin R21, R1
cos R22, R1
tan R23, R1
asin R24, R1
acos R25, R1
atan R26, R1

; ===== Логика =====
and R30, R1, R2
and R31, R1, #1

or R32, R1, R3
or R33, R1, #2

xor R34, R1, R2
xor R35, R1, #3

not R36, R1           ; unary (A := ~B)

; ===== Сравнения =====
eq R40, R1, R2
eq R41, R1, #10

ne R42, R1, R3
ne R43, R1, #5

gt R44, R1, R2
gt R45, R1, #8

ge R46, R1, R2
ge R47, R1, #10

lt R48, R1, R2
lt R49, R1, #20

le R50, R1, R2
le R51, R1, #10

; ===== Время и дата (не требуют C) =====
time R60, R0          ; A := seconds since day start
date R61, R0
tod R62, R0
dt R63, R0

add_time R64, R1, #60 ; добавление времени (C immediate)
add_time R65, R1, R2  ; или C как регистр

sub_time R66, R1, #30
sub_time R67, R1, R2

year R68, R0
month R69, R0
day R70, R0
hour R71, R0
minute R72, R0
second R73, R0

; ===== Строки =====
; предположим, в памяти по адресам R100..R102 уже лежат строки (демо)
; len: B = адрес строки
len R80, R100

concat R81, R200, R100   ; concat dest@=R200 (addr in R200), src addr in R100 (C must be reg)
; вариант для left/right/mid/insert/delete/replace (C может быть imm)
left R82, R200, #5
left R83, R200, R2

right R84, R200, #3
right R85, R200, R2

mid R86, R200, #2
mid R87, R200, R2

insert R88, R200, #1
insert R89, R200, R2

delete R90, R200, #2
delete R91, R200, R2

replace R92, R200, #4
replace R93, R200, R2

; ===== Таймеры (TON/TOF/TP) =====
; Для таймеров C используется как preset (или immediate id как C? - в реализации id приходит из IMM8/C?),
; но у тебя op_ton проверяет FIMM(i) перед использованием — поэтому C здесь мы даём как immediate id.
; Конвенция: A = output reg (Q), B = input reg (IN), C (if immediate) = id, RC used as PT register in code? 
; (в твоей реализации pt = vm->reg[RC(i)]) — поэтому здесь PT передаётся в RC (т.е. третий operand must be reg),
; но op_ton earlier required FIMM(i) true and id = IMM8(i). To demonstrate both styles:

; TON with id in immediate (C as immediate) — input in RB, preset in RC (register)
ton R100, R2, #0      ; output R100 := ton for timer id 0 (preset in R? see code uses RC for pt)
; Alternative style: use id stored in RA (non-immediate id)
; To use id from RA we need to put id into RA beforehand:
add R110, R0, #1      ; R110 = 1 (id)
ton R111, R2, R110    ; but in our impl id from RA(i) when FIMM==0 -> RA holds id; here RA=R111 used both => ambiguous.
; (Практически для таймеров лучше использовать immediate id: ton R_out, R_in, #id with preset in RC register)

tof R112, R2, #1
tp R113, R2, #2

; ===== Счётчики CTU/CTD/CTUD =====
; CTU/CTD: RA contains id when not immediate (we prefer immed)
ctu R120, R2, #3
ctu R121, R2, R3    ; also reg form (here C is register holding preset)

ctd R122, R2, #2
ctd R123, R2, R3

ctud R124, R10, R11  ; ctud requires C to be register (C_MUST_REG): RB = up, RC = down, RA holds id/register->value

; ===== limit / sel / mux =====
limit R130, R1, #0   ; clamp R130 (from RA) between lo=B(R1) and hi=#0 (example)
limit R131, R1, R2

sel R132, R1, R200   ; sel: base must be reg (C is reg pointing to base table)
mux R133, R200, #1   ; mux: base in RB, index in C (immediate)
mux R134, R200, R2   ; mux: index in register

; ===== IEC/SCADA ops (edges, latches, demux) =====
; rising_edge: A=output, B=input signal, C = #id or reg (id)
rising_edge R140, R2, #0
rising_edge R141, R2, R10   ; if R10 contains id (non-immediate)

falling_edge R142, R2, #1
falling_edge R143, R2, R10

edge_both R144, R2, #2
edge_both R145, R2, R10

; RS / SR latches:
; Convention used: A = output (Q), RB = S input, RC = R input. id can be immediate (#) or in RA.
rs_latch R150, R2, R3         ; id from RA (RA used as output too) — typical: RA used as storage index
rs_latch R151, R2, #1         ; immediate id = 1

sr_latch R152, R2, R3
sr_latch R153, R2, #2

; demux: A = value reg to write, B = base address reg, C = index (reg or #)
demux R160, R200, #0
demux R161, R200, R2

; ===== JMP-инструкции =====
; jmp: target by B register or immediate (#offset)
jmp R0, R50, #8      ; jump to PC = signext(#8)
jmp R0, R50, R2      ; jump to PC = Bv(vm,i) (value of R2)  — here B is R50? (we pass B as second operand)

; jmp_if: tests C (Cv), then target in B or imm
jmp_if R0, R100, #16    ; if R(C) != 0 then PC = #16
jmp_if R0, R100, R2     ; if R(C) != 0 then PC = R2

jmp_if_not R0, R100, #20
jmp_if_not R0, R100, R2

; ===== Прочее =====
exit R0, R0, #0      ; exit with code #0 (or if Av==0 -> IMM8 used). Using first form: A=R0 => treat special in your op_exit
halt R0, R0, R0      ; (может быть переписан в exit 0 ассемблером)
nop R0, R0, R0

; end of examples
