; timer_test_multicycle.asm
; Тест TON / TOF / TP — заставляет тест длиться > 1 VM cycle (1000 ms)
; Подход:
;  - id хранится в R20 (чтобы не терять его при перезаписи RA)
;  - перед каждым вызовом копируем id -> RA (op_ton читает id из RA)
;  - между стартом таймера и проверкой делаем много NOP (чтобы пройти >1000ms)
;  - в конце — loop (jmp) чтобы программа не завершилась (прерывай Ctrl-C)

; Предполагается: assembler синтаксис: <op> A B C
; - A, B, C — регистры либо C может быть immediate (#n)
; - op_ton/op_tof/op_tp в текущем билде читают id из рег[RA], PT из C (imm или reg)
;   (если ты уже применил изменение семантики — скажи, я подправлю под новую семантику)

; ---------------------------
; Подготовка
; ---------------------------
sub  R2, R0, R0        ; R2 := 0  (zero)
; сохраняем id'ы в R20..R22 (не будут перезаписываться напрямую)
add  R20, R2, #2       ; timer id = 2 (TON test)
add  R21, R2, #3       ; timer id = 3 (TOF test)
add  R22, R2, #4       ; timer id = 4 (TP test)

; ---------------------------
; TON тест (PT = 200 ms)
; ---------------------------
; Стадия 0: инициация (IN=0 -> затем фронт -> старт)
; Ставим IN=0, PT in R12
add  R11, R2, #0       ; R11 := 0 (IN)
add  R12, R2, #200     ; R12 := 200 (PT)
add  R10, R20, #0      ; R10 := R20 (скопировали id -> RA)
ton  R10, R11, R12     ; стартуем TON (R10 будет перезаписан Q)
                       ; на этом моменте Q == 0 (незашёл PT)

; Фронт: поднять IN -> вызываем TON чтобы зафиксировать старт
add  R11, R2, #1       ; R11 := 1 (rising edge)
add  R10, R20, #0      ; reload id -> R10
ton  R10, R11, R12     ; вызов записывает Q в R10 (пока 0), таймер стартует

; Тут ставим много NOP чтобы потратить >1000 ms (примерно)
; 120 * ~10 ms = ~1200 ms (при clock_rate_hz = 100)
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop

; После «долгих» NOP — снова подгружаем id -> читаем Q
add  R10, R20, #0
ton  R10, R11, R12     ; теперь если прошло >=200 ms с фронта — R10 == 1

; ---------------------------
; TOF тест (PT = 300 ms)
; ---------------------------
; устанавливаем IN = 1, PT in R12
add  R11, R2, #1       ; IN := 1
add  R12, R2, #300     ; PT := 300
add  R10, R21, #0      ; load id (R21=3) -> R10
tof  R10, R11, R12     ; Q -> R10 (should be 1 while IN=1)

; делаем спуск IN -> запуск таймера выключения
add  R11, R2, #0       ; IN := 0 (falling)
add  R10, R21, #0
tof  R10, R11, R12     ; запускаем отсчёт выключения (R10 пока 1)

; снова много NOP (больше 1000 ms)
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop

; Проверка TOF после задержки
add  R10, R21, #0
tof  R10, R11, R12     ; если прошло >=300ms с падения — R10 == 0

; ---------------------------
; TP тест (pulse 200 ms)
; ---------------------------
add  R12, R2, #200     ; PT := 200
add  R11, R2, #0       ; IN := 0
add  R10, R22, #0      ; id -> R10
add  R11, R2, #1       ; rising edge
tp   R10, R11, R12     ; TP generates pulse (R10 -> 1 for ~200ms)

; немного NOP'ов, затем ещё вызов TP
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop

add  R10, R22, #0
tp   R10, R11, R12     ; вызов игнорируется, если импульс ещё идёт

; ---------------------------
; Завершающая петля — чтобы программа не завершилась
; ---------------------------
jmp  R2, #0   ; бесконечно прыгаем на начало (R2 == 0) — просто заглушка
