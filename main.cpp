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

using namespace std;

class Application
{
public:
    Application()
    {
        timer.onTimeout([]() { std::cout << "Application timer 3 event!" << std::endl; });
        timer.start(200);
    }

private:
    LTimer timer;
};

int main()
{
    LEventLoop loop;

    LGpioPin ledPin("/dev/gpiochip4", 17);
    ledPin.setDirection(LGpioPin::Output);

    LTimer timer_1;
    timer_1.onTimeout([]() { std::cout << "Timer_1 event callback!" << std::endl; });
    timer_1.start(250);

    LTimer timer_2;
    timer_2.onTimeout([]() { std::cout << "Timer_2 event!" << std::endl; });
    timer_2.start(150);

    Application app;

    return loop.exec();
}
