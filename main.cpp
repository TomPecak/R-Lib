//Linux
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

//C++
#include <cerrno>
#include <cstring>
#include <iostream>

#include "./src/LEventLoop.hpp"
#include "./src/LGpioPin.hpp"
#include "./src/LTimer.hpp"
#include "./src/LUdpSocket.hpp"

using namespace std;

class Application
{
public:
    Application()
        : tick_count_(0)
    {
        timer.onTimeout([this]() { this->handle_timeout(); });
        timer.start(1);
    }

    void handle_timeout()
    {
        tick_count_++;

        // Co 1000 wywołań (w przybliżeniu co 1 sekundę) wypisujemy informację na ekran
        if (tick_count_ % 1000 == 0) {
            std::cout << "LTimer Wywolanie nr: " << tick_count_ << " (Minelo ok. "
                      << tick_count_ / 1000 << " s)" << std::endl;
        }
    }

private:
    LTimer timer;
    uint64_t tick_count_;
};

int main()
{
    LEventLoop loop;

    // LGpioPin ledPin("/dev/gpiochip4", 17);
    // ledPin.setDirection(LGpioPin::Output);

    //auto app_3 = new Application();

    LTimer timer_1;
    timer_1.onTimeout([]() {
        std::cout << "Quit application!" << std::endl;
        LEventLoop::quit();
    });
    timer_1.start(5 * 1000);

    LTimer timer_2;
    timer_2.onTimeout([]() { std::cout << "Timer_2 event!" << std::endl; });
    timer_2.start(150);

    Application app;
    Application app_2;

    return loop.exec();
}
