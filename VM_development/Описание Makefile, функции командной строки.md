
# Makefile — документация для проекта VM32

## Назначение

Makefile автоматизирует сборку проекта VM32 и утилит:

- собирает виртуальную машину **vm32** и вспомогательные модули (`funcs.c`, `debug.c`, `timers.c`, `hashing.c`, `cleanup.c`);
    
- собирает ассемблер-конвертер **converter** (`converter.c`) для превращения `program.asm` → `program.bin`;
    
- выполняет полный пайплайн: сборка → конвертация → запуск VM, с сохранением лога в `build/`;
    
- поддерживает отладочную сборку, очистку артефактов и частичные сборки.
    

---

## Структура проекта (важные пути)

```
.
├── build/                 # артефакты сборки (.o, бинарники, program.bin, лог)
│   ├── vm32
│   ├── converter
│   ├── *.o
│   ├── program.bin
│   └── vm32_log.txt
├── include/               # заголовки
├── vm32.c
├── funcs.c
├── debug.c
├── hashing.c
├── timers.c
├── cleanup.c
├── converter.c
├── program.asm
└── Makefile
```

> По умолчанию все конечные артефакты складываются в директорию `build/`.

---

## Переменные Makefile (коротко)

- `CC` — компилятор (по умолчанию `gcc`)
    
- `CFLAGS` — флаги компилятора (`-std=c11 -O2 -Wall -Wextra -Iinclude`)
    
- `LDFLAGS` — флаги линковщика (например `-lm -latomic`)
    
- `BUILD_DIR` — директория для артефактов (`build`)
    
- `VM_SRCS`, `VM_OBJS` — исходники и объектники для VM
    
- `CONVERTER_SRCS`, `CONVERTER_OBJS` — исходники и объектники для конвертера
    
- `ASM_SRC` — входной ASM-файл (`program.asm`)
    
- `BIN_OUT` — путь до сгенерированного бинарника (`build/program.bin`)
    
- `VM_LOG` — путь до лога выполнения (`build/vm32_log.txt`)
    

---

## Основные цели (targets)

### `make` / `make all`

Собирает всё: `vm32` и `converter` (артефакты в `build/`).

### `make convert`

Запускает только конвертер и переводит `program.asm` → `build/program.bin`:

```bash
./build/converter program.asm build/program.bin
```

### `make vm`

Запускает виртуальную машину с бинарником `build/program.bin` и перенаправляет вывод в лог:

```bash
./build/vm32 build/program.bin > build/vm32_log.txt
```

### `make run` (рекомендуемый для тестирования)

Полный пайплайн: сборка → конвертация → запуск VM. Итоговые логи и бинарник лежат в `build/`.  
Выполнение примерно такое:

1. `make` (собирает `vm32` и `converter`)
    
2. `./build/converter program.asm build/program.bin`
    
3. `./build/vm32 build/program.bin > build/vm32_log.txt`
    

### `make build-bin`

Только сборка бинарника `program.bin` (вызывает `convert`); полезно при отладке конвертера.

### `make debug`

Собирает проект с отладочными флагами (`-g -O0`) — затем можно запускать `make run` для отладочного прогона.

### `make clean`

Удаляет `build/` (или `*.o`, бинарники и `program.bin` в старой версии). Используйте перед `make debug` или перед `rebuild`.

### `make rebuild`

`clean` + `all` — полная пересборка с нуля.

---

## Примеры использования

Собрать проект:

```bash
make
```

Собрать и выполнить полный цикл (build → convert → run):

```bash
make run
# затем смотреть лог:
less build/vm32_log.txt
```

Только конвертация (ASM → BIN):

```bash
make convert
# результат: build/program.bin
```

Отладочная сборка и запуск:

```bash
make debug
make run
# после этого можно запустить в gdb:
gdb --args build/vm32 build/program.bin
```

Очистить артефакты:

```bash
make clean
```

---

## Что лежит в `build/`

- `build/*.o` — объектные файлы для всех исходников;
    
- `build/vm32` — исполняемый файл виртуальной машины;
    
- `build/converter` — исполняемый файл конвертера;
    
- `build/program.bin` — сгенерированный бинарник из `program.asm`;
    
- `build/vm32_log.txt` — лог выполнения VM (при использовании `make vm` или `make run`).
    

---

## Как добавлять новые исходники

1. Добавьте имя файла в переменную `VM_SRCS` (или `CONVERTER_SRCS`), например:
    
    ```make
    VM_SRCS := vm32.c funcs.c debug.c hashing.c timers.c cleanup.c newmodule.c
    ```
    
2. Убедитесь, что для заголовков есть `#include` в исходниках.
    
3. Выполните `make` — Make автоматически пересоберёт только изменённые части.
    

---

## Рекомендации для разработки и тестирования

- Для отлова ошибок управления памятью компилируйте с ASAN:
    
    ```bash
    make debug
    # затем добавить в CFLAGS: -fsanitize=address,undefined
    ```
    
    (Можно добавить отдельную цель `asan` в Makefile.)
    
- Для профилирования/микробенчмарков запускайте VM с разными входами и сохраняйте логи (`build/vm32_log.txt`) для последующего анализа.
    
- При изменении формата бинарника или интерфейса конвертера — обновляйте `build-bin` и добавляйте тесты.
    

---

## PHONY цели

Makefile содержит `.PHONY` для целей, которые не соответствуют реальным файлам:

```make
.PHONY: all clean debug convert vm run rebuild build-bin
```

---

## Примечания и особенности

- Makefile построен так, чтобы минимизировать лишние пересборки: объектные файлы пересобираются только при изменении соответствующих исходников или заголовков.
    
- Все исполняемые файлы кладутся в `build/` — это упрощает управление артефактами и позволяет хранить исходники в корне проекта.
    
- Лог выполнения VM записывается в `build/vm32_log.txt` (удобно для парсинга результатов тестов и отчетов).
    

---