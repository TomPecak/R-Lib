# R-Lib

R-Lib is a lightweight C++ wrapper and bridge over native Linux APIs, designed for high-performance and resource-constrained environments such as embedded systems, control processes, and IoT devices.

The library is inspired by the API design patterns of the Qt framework (event loops, callback-driven components, and non-blocking I/O) but targets Linux specifically. By avoiding cross-platform abstraction layers, it aims to leverage native Linux system calls directly to maximize execution efficiency.

## Core Features

- **Linux-Native Performance:** Uses low-level system calls (`epoll`, `timerfd`) without translation or emulation layers.
- **Qt-Like API Style:** Straightforward, callback-based interface designed to help developers establish a clean class layout for event-driven applications.
- **Modern C++ Design:** Written in C++17, utilizing standard mechanisms like `std::function` for callbacks and `thread_local` pointers for implicit context management.
- **Resource Efficiency:** Low memory footprint, safe lifetime management, and clean cleanup mechanisms.

## Project Roadmap

The library is designed to expand into a complete toolkit for embedded Linux hardware and communication protocols, targeting:
- **GPIO Interface:** Modern Linux character device GPIO API (`gpiod`).
- **Networking:** TCP and UDP sockets.
- **Serial Communication:** UART/Serial port handling (`termios`).
- **Timers:** High-resolution periodic and single-shot timers via `timerfd`.

---

## Architectural Highlights

### `LEventLoop`
An `epoll`-based event loop running per-thread. It implements a static registry pattern where the current thread-local loop is automatically discovered by newly instantiated handlers, avoiding the need to pass event loop pointers around.

### `LTimer`
A timer implementation based on `timerfd_create` and `timerfd_settime`. It integrates into `LEventLoop` by implementing the `LEpollHandler` interface. Its destructor safely queries the current thread-local event loop to perform clean unregistration prior to closing the file descriptor, mitigating use-after-free scenarios.

---

## Quick Start Example

### 1. Basic Timer Example
The following example demonstrates how to set up the event loop and instantiate multiple concurrent timers.

```cpp
#include <iostream>
#include "LEventLoop.hpp"
#include "LTimer.hpp"

class Application
{
public:
    Application()
    {
        timer.onTimeout([]() { std::cout << "Application timer event!" << std::endl; });
        timer.start(200); // 200 ms interval
    }

private:
    LTimer timer;
};

int main()
{
    LEventLoop loop;

    LTimer timer_1;
    timer_1.onTimeout([]() { std::cout << "Timer_1 event callback!" << std::endl; });
    timer_1.start(100); // 100 ms interval

    LTimer timer_2;
    timer_2.onTimeout([]() { std::cout << "Timer_2 event!" << std::endl; });
    timer_2.start(150); // 150 ms interval

    Application app;

    return loop.exec();
}
```

### 2. UDP Sender Example
This example demonstrates how to combine LTimer and LUdpSocket to create an asynchronous UDP sender that transmits a message every second to localhost on port 5555.

```cpp
#include <cerrno>
#include <cstring>
#include <iostream>

#include <LEventLoop.hpp>
#include <LTimer.hpp>
#include <LUdpSocket.hpp>

using namespace std;

class UdpSender
{
public:
    UdpSender()
        : counter(0)
    {
        timer.onTimeout(this, &UdpSender::sendData);
        timer.start(1000); // 1000 ms = 1 second
    }

    void sendData()
    {
        counter++;
        std::string message = "Message no. " + std::to_string(counter) + " from R-Lib!\n";
        socket.writeDatagram(message.c_str(), message.length(), "127.0.0.1", 5555);
    }

private:
    LUdpSocket socket;
    LTimer timer;
    int counter;
};

int main()
{
    LEventLoop loop;
    UdpSender sender;
    return loop.exec();
}
```


## Building the Project
The library uses CMake as its build system.

### Prerequisites
- A Linux-based operating system.
- A C++ compiler supporting at least C++17 (e.g., GCC or Clang).
- CMake 3.16 or newer.

### Build Steps
1.  Clone or copy the project files to your local environment.
2. Create a build directory and run CMake:
```bash
mkdir build && cd build
cmake ..
```
3. Compile the executable:

```bash
make
```

### Run the demo application:

```bash
./R-Lib
```

