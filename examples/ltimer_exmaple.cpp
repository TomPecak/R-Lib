#include <cerrno>
#include <cstring>
#include <iostream>

#include "LEventLoop.hpp"
#include "LTimer.hpp"

using namespace std;

class Application
{
public:
    Application()
    {
        timer_3.onTimeout([]() { std::cout << "Application timer 3 event!" << std::endl; });
        timer_3.start(200);

        timer_4.onTimeout(this, &Application::handleTimer_4);
        timer_4.start(250);
    }

    void handleTimer_4() { std::cout << "Application timer 4 event!" << std::endl; }

private:
    LTimer timer_3;
    LTimer timer_4;
};

int main()
{
    LEventLoop loop;

    LTimer timer_1;
    timer_1.onTimeout([]() { std::cout << "Timer_1 event callback!" << std::endl; });
    timer_1.start(250);

    LTimer timer_2;
    timer_2.onTimeout([]() { std::cout << "Timer_2 event!" << std::endl; });
    timer_2.start(150);

    Application app;

    return loop.exec();
}