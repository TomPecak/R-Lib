#pragma once

#include <cstdint>
#include <functional>

#include "LEventLoop.hpp"

class LTimer : public LEpollHandler
{
public:
    LTimer();
    ~LTimer() override;

    void start(uint64_t interval);
    void setInterval(uint64_t msec);
    void start();
    void stop();

    //Callbacks
    void onTimeout(std::function<void()> callback);

protected:
    void handleEpollEvent(uint32_t events) override;

private:
    int m_timer_fd;
    uint64_t m_interval_ms;
    std::function<void()> m_callback;
};
