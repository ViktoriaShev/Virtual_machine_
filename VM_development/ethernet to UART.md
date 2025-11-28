rrВот **подборка готовых решений (open-source проекты и библиотеки)** для 
организации **Ethernet-to-UART (serial over TCP/IP)**, чтобы подключаться к твоим tty-портам по сети.

---

###  **1. `ser2net` (Linux)**

🔗 [GitHub: ser2net](https://github.com/cminyard/ser2net)

- **Назначение**: классический Linux-демон для проброса ttyS*, ttyUSB*, ttyACM* по TCP/IP
    
- **Установка (Debian/Ubuntu)**:
    

```bash
sudo apt install ser2net
```

- **Конфигурация** (пример для /dev/ttyUSB0 на порту 5000):
    

В файле `/etc/ser2net.conf` (или `/etc/ser2net/ser2net.conf` в новых версиях):

```
5000:telnet:0:/dev/ttyUSB0:115200 8DATABITS NONE 1STOPBIT
```

- **Запуск**:
    

```bash
sudo systemctl restart ser2net
```

- После этого ты сможешь подключиться к порту с другого устройства:
    

```bash
telnet <ip-адрес> 5000
```

или через `netcat`, или любым TCP-клиентом, и работать как через UART.

---

### ✅ **2. `socat`**

🔗 [socat project](http://www.dest-unreach.org/socat/)

- Универсальный инструмент для проброса любых потоков, включая UART ⇔ TCP:
    

```bash
socat tcp-l:5000,reuseaddr,fork file:/dev/ttyUSB0,raw,echo=0,b115200
```

- **Плюсы**: не требует конфигурационных файлов, запускается одной строкой
    
- **Минусы**: в роли постоянного сервиса лучше использовать `ser2net` или завернуть `socat` в systemd-unit.
    

---

### ✅ **3. ESP32-based проекты Ethernet-to-UART (если требуется железное решение)**

Если хочешь аппаратный **Ethernet-to-Serial bridge**, без Linux ПК:

- **ESP32 + W5500 (Ethernet)** или ESP32 с WiFi
    
- Готовый проект: **ESP32-TCP-Bridge**
    

🔗 [ESP32-TCP-Bridge GitHub](https://github.com/khoih-prog/ESP32_W5500_Ethernet_TCP_ASYNC)

- Работает в режиме TCP сервер ⇔ UART
    
- Ты подключаешься по TCP – данные уходят в UART и обратно.
    

---

### ✅ **4. Commercial open-source hybrid: Lantronix Device Servers**

Если нужно промышленное решение – Lantronix UDS-1100, UDS-2100 и аналоги. Они проприетарные, но стандартно работают как ser2net.

---

### 🔧 **Твоя задача**

Так как у тебя **Linux (Mint)** и локальные tty-порты, **лучший вариант – `ser2net`**:

1. Установи:
    

```bash
sudo apt install ser2net
```

2. Настрой в `/etc/ser2net.conf`.
    
3. Перезапусти сервис и проверь подключение по telnet или netcat.
    

---

📌 **Если хочешь**, могу подготовить тебе **полный unit-файл systemd и конфигурацию ser2net** для всех твоих ttyUSB-портов с фиксированными TCP-портами и удобным описанием для документации проекта. Сообщи, если нужно.


### 📌 Общая архитектура:

1. **Сервер (на втором компьютере)**:
    
    - Читает и пишет данные из/в `ttyS1`.
        
    - Открывает TCP-сервер (например, на порту 5555).
        
    - Принимает подключения от клиента и передаёт все данные между TCP и ttyS1.
        
2. **Клиент (на первом компьютере)**:
    
    - Подключается к TCP-серверу.
        
    - Создаёт **виртуальный COM-порт** (с помощью `pty` или `socat`/`tty0tty`).
        
    - Форвардит данные между этим виртуальным портом и TCP.
        

---

## ✅ 1. Сервер: `serial_tcp_server.c` (на втором компьютере)

```c
// serial_tcp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <netinet/in.h>
#include <pthread.h>

#define SERIAL_PORT "/dev/ttyS1"
#define TCP_PORT 5555
#define BUFFER_SIZE 256

int serial_fd;

void configure_serial(int fd) {
    struct termios tty;
    tcgetattr(fd, &tty);

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag = CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tty);
}

void* serial_to_tcp(void* arg) {
    int client_fd = *(int*)arg;
    char buf[BUFFER_SIZE];
    int n;

    while ((n = read(serial_fd, buf, sizeof(buf))) > 0) {
        write(client_fd, buf, n);
    }

    close(client_fd);
    return NULL;
}

void* tcp_to_serial(void* arg) {
    int client_fd = *(int*)arg;
    char buf[BUFFER_SIZE];
    int n;

    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
        write(serial_fd, buf, n);
    }

    close(client_fd);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (serial_fd < 0) {
        perror("Failed to open serial port");
        exit(1);
    }

    configure_serial(serial_fd);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(TCP_PORT);

    bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(server_fd, 1);

    printf("Waiting for client...\n");
    client_fd = accept(server_fd, (struct sockaddr*)&cli_addr, &clilen);
    printf("Client connected\n");

    pthread_t t1, t2;
    pthread_create(&t1, NULL, serial_to_tcp, &client_fd);
    pthread_create(&t2, NULL, tcp_to_serial, &client_fd);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    close(serial_fd);
    close(server_fd);
    return 0;
}
```

Соберите:

```bash
gcc -o serial_tcp_server serial_tcp_server.c -lpthread
```

---

## ✅ 2. Клиент: создание виртуального tty + подключение

Для реализации виртуального tty-порта вы можете использовать:

### Вариант A: `socat` (рекомендуется — быстро и без кода)

```bash
socat -d -d PTY,link=/dev/ttyVIRTUAL,raw TCP:IP_СЕРВЕРА:5555
```

- `IP_СЕРВЕРА` — IP-адрес второго одноплатника.
    
- После запуска, у вас появится виртуальный `/dev/ttyVIRTUAL`, который можно использовать как обычный COM-порт.
    

### Вариант B: C-клиент + `pty`

Если хотите всё на C, я могу также предоставить клиентскую программу, создающую `pty`, но `socat` — гораздо проще.

---

## ✅ Проверка работы

На втором компьютере:

```bash
./serial_tcp_server
```

На первом:

```bash
socat -d -d PTY,link=/dev/ttyVIRTUAL,raw TCP:192.168.1.100:5555
```

Теперь `/dev/ttyVIRTUAL` на первом будет фактически транслироваться в `ttyS1` на втором.

---

## 🔄 Нужно C-клиент вместо socat?

Если вы хотите полностью обойтись без `socat`, могу написать C-программу, создающую виртуальный tty с помощью `openpty()` и форвардом TCP. Скажите — я добавлю.

---

Нужна помощь с запуском, автозагрузкой или systemd-сервисами — тоже подскажу.

# Команды 
```
 echo -ne '\x03\x06\x00\x68\x00\x7B\x49\xD7' > /dev/ttyS1
```

```
 ./main ttyS1 5555 --baud=B115200 --stop=1 --parity=None --bits=8 rts-delay=100

```

```
arm-gcc -o main main.c -lpthread
```

```
sudo lsof -i :5555

```

```
dd if=/dev/ttyS10 bs=1 count=2 | hexdump -C
```

+------------------+          TCP           +--------------------+       RS485       +---------------------+
| TCP Client (CLI) | <-------------------> | RS485-TCP Bridge    | <=============>   | RS485 Device (e.g.) |
|   or netcat      |                      | (write_rs485)       |                   |  Modbus RTU Slave   |
+------------------+                      +--------------------+                   +---------------------+
