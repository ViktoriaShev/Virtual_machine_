Ниже — полностью готовая, аккуратно оформленная **документация к твоему Makefile**, написанная как техническое описание для разработчика.  
Если хочешь — могу добавить ASCII-диаграмму зависимостей или сделать README.md.

---

#  **Документация по Makefile для проекта VM32**

## 📌 Назначение Makefile

Данный `Makefile` автоматизирует сборку проекта, состоящего из:

- виртуальной машины **VM32**
    
- вспомогательных модулей (`funcs.c`, `debug.c`)
    
- ассемблера **converter**, который превращает `.asm` → `.bin`
    

Makefile обеспечивает:

- автоматическую компиляцию `.c` → `.o`
    
- линковку исполняемых файлов
    
- пересборку по изменённым файлам
    
- удобные команды (`clean`, `rebuild`, `build-bin`)
    

---

# 🛠 Общая структура

Makefile определяет:

### **Компилятор и флаги**

```make
CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude
```

- Используется стандарт C11
    
- Оптимизация `-O2`
    
- Включены предупреждения
    
- Включён путь `include/` для заголовков
    

---

# 📦 Цели сборки

## **1. Цель по умолчанию — `all`**

```make
all: vm32 converter
```

Запускает полную сборку виртуальной машины **и** ассемблера.

---

# 🧩 Сборка виртуальной машины VM32

## Файлы VM:

```make
VM_OBJS = vm32.o funcs.o debug.o
```

## Исполняемый файл:

```make
vm32: $(VM_OBJS)
	$(CC) $(CFLAGS) -o vm32 $(VM_OBJS) -lm
```

- Компиляет и линкует `vm32`
    
- Добавляет `-lm` для математических функций (sin, cos и т.п.)
    

---

# ⚙ Правила генерации `.o` для VM

### **vm32.o**

```make
vm32.o: vm32.c include/vm32.h include/funcs.h include/debug.h
	$(CC) $(CFLAGS) -c vm32.c
```

Цель пересобирается при изменениях:

- исходника `vm32.c`
    
- заголовков `vm32.h`, `funcs.h`, `debug.h`
    

---

### **funcs.o**

```make
funcs.o: funcs.c include/funcs.h include/vm32.h
	$(CC) $(CFLAGS) -c funcs.c
```

---

### **debug.o**

```make
debug.o: debug.c include/debug.h include/vm32.h
	$(CC) $(CFLAGS) -c debug.c
```

---

# 🧾 Сборка ассемблера (converter)

Ассемблер собирается из одного файла:

```make
converter: converter.o
	$(CC) $(CFLAGS) -o converter converter.o
```

---

### `.o` для converter

```make
converter.o: converter.c include/vm32.h
	$(CC) $(CFLAGS) -c converter.c
```

---

# 🧹 Дополнительные цели

## **Полная пересборка**

```make
rebuild: clean all
```

Удаляет все артефакты и собирает заново.

---

## **Очистка проекта**

```make
clean:
	rm -f *.o vm32 converter program.bin
```

Удаляет:

- объектные файлы `.o`
    
- бинарники `vm32` и `converter`
    
- собранный бинарник `program.bin`
    

---

## **Сборка ASM → BIN**

```make
build-bin: converter program.asm
	./converter program.asm program.bin
	@echo "✓ program.bin создан"
```

Удобная команда, превращающая `program.asm` в `program.bin`.  
Используется при тестировании VM.

---

# ✨ PHONY цели

```make
.PHONY: all clean rebuild build-bin
```

Сообщает make, что это не файлы, а команды.

---

# 📑 Краткий список команд

| Команда          | Описание                          |
| ---------------- | --------------------------------- |
| `make`           | Полная сборка VM и ассемблера     |
| `make vm32`      | Собрать только виртуальную машину |
| `make converter` | Собрать только ассемблер          |
| `make clean`     | Очистить проект                   |
| `make rebuild`   | Полная пересборка                 |
| `make build-bin` | Сборка program.asm → program.bin  |

---
