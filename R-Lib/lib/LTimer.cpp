#include "LTimer.hpp"

LTimer::LTimer() {}

void LTimer::start(uint64_t interval)
{
    setInterval(interval);
    start();
}

void LTimer::setInterval(uint64_t msec) {}

void LTimer::start() {}
