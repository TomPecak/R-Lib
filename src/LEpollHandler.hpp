#pragma once

#include <cstdint>

class LEpollHandler
{
public:
    virtual ~LEpollHandler() = default;
    virtual void handleEpollEvent(uint32_t events) = 0;
};
