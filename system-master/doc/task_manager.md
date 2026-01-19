# Task Manager — Architecture and Design

## 1. Назначение

Task Manager является логическим уровнем над Scheduler в проекте **system**.

Он вводится на ШАГЕ 7 архитектурного плана и решает следующие задачи:
- группировка исполняемых единиц (ITickable)
- формирование логических задач (Task)
- подготовка архитектуры к IEC Tasks и Program execution

Task Manager **не заменяет Scheduler**, а расширяет его возможности.

---

## 2. Основные понятия

### 2.1 ITickable

Минимальная исполняемая единица системы.

```cpp
struct ITickable {
    virtual ~ITickable() = default;
    virtual void tick() = 0;
};
```

ITickable:
- не владеет временем
- не знает о Scheduler
- не управляет потоками

---

### 2.2 Task

Task — это **группа ITickable**, исполняемая как единое целое.

Свойства Task:
- имеет имя (для диагностики)
- реализует ITickable
- последовательно вызывает tick() у своих unit’ов

Task:
- не владеет unit’ами
- не управляет временем
- не знает о Scheduler

---

## 3. TaskManager

TaskManager отвечает за:
- создание Task
- хранение Task
- регистрацию Task в Scheduler

Ключевая идея:
> Scheduler исполняет Task,  
> Task исполняет ITickable.

---

## 4. Связь со Scheduler

TaskManager **не создаёт Scheduler** и **не владеет им**.

Связывание происходит через явный API:

```cpp
void bindScheduler(Scheduler* scheduler);
```

Это:
- сохраняет инверсию зависимостей
- упрощает тестирование
- исключает скрытые зависимости

---

## 5. TaskManagerComponent

TaskManagerComponent:
- является компонентом runtime
- получает Scheduler через SystemBus
- связывает TaskManager и Scheduler
- регистрирует TaskManager как сервис

Таким образом:
- Core-компоненты получают TaskManager через SystemBus
- прямой доступ к Scheduler не требуется

---

## 6. Жизненный цикл

### init()
- получение Scheduler
- bindScheduler()
- регистрация TaskManager в SystemBus

### start()
- регистрация всех Task в Scheduler

### stop()
- на текущем этапе не содержит логики

---

## 7. Использование в компонентах

Типовой сценарий:
1. Компонент получает TaskManager
2. Создаёт Task
3. Добавляет ITickable в Task

Пример (упрощённо):

```cpp
auto& task = taskManager->createTask("MainTask");
task.add(unit);
```

---

## 8. Архитектурные ограничения

Осознанные ограничения текущей версии:
- нет приоритетов Task
- нет периодов
- нет динамического удаления Task
- нет статистики выполнения

Все ограничения снимаются **эволюционно**, без изменения базовой модели.

---

## 9. Связь с IEC Runtime

Task Manager является прямым предшественником:
- IEC Task
- IEC Program
- IEC Function Block execution

В дальнейшем:
- Task станет IEC Task
- ITickable — Program / FB invocation
- Scheduler — scan cycle

---

## 10. Статус

Статус: **реализован (базовая версия)**

Следующий архитектурный шаг:
- IEC Task / Program skeleton
