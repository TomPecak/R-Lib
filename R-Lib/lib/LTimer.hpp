#pragma once

#include <cstdint>

class LTimer
{
public:
    LTimer();

    void start(uint64_t interval);
    void setInterval(uint64_t msec);
    void start();
};
