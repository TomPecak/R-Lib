#include "LTimer.hpp"

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h> // close, read

#include <cerrno>  // errno
#include <cstring> // strerror
#include <iostream>

LTimer::LTimer()
    : m_timer_fd(-1)
    , m_interval_ms(0)
{
    m_loop = LEventLoop::current();

    if (m_loop == nullptr) {
        std::cerr << "CRITICAL: Brak utworzonej LEventLoop w tym wątku!" << std::endl;
    }

    m_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (m_timer_fd == -1) {
        std::cerr << "Błąd timerfd_create: " << strerror(errno) << std::endl;
        return;
    }

    m_loop->registerHandler(m_timer_fd, EPOLLIN, this);
}

LTimer::~LTimer()
{
    if (m_timer_fd != -1) {
        if (m_loop) {
            m_loop->unregisterHandler(m_timer_fd);
        }
        close(m_timer_fd);
        m_timer_fd = -1;
    }
}

void LTimer::start(uint64_t interval)
{
    setInterval(interval);
    start();
}

void LTimer::setInterval(uint64_t msec)
{
    m_interval_ms = msec;
}

void LTimer::start()
{
    if (m_interval_ms == 0 || m_timer_fd == -1)
        return;

    // Set time
    struct itimerspec ts;
    ts.it_value.tv_sec = m_interval_ms / 1000;
    ts.it_value.tv_nsec = (m_interval_ms % 1000) * 1000000;
    ts.it_interval.tv_sec = ts.it_value.tv_sec;
    ts.it_interval.tv_nsec = ts.it_value.tv_nsec;

    // Start timer in kernel
    if (timerfd_settime(m_timer_fd, 0, &ts, nullptr) == -1) {
        std::cerr << "Błąd timerfd_settime (start): " << strerror(errno) << std::endl;
    }
}

void LTimer::stop()
{
    if (m_timer_fd == -1)
        return;

    // set time to 0
    struct itimerspec ts = {}; // Inicjalizacja same zera

    // stop timer
    if (timerfd_settime(m_timer_fd, 0, &ts, nullptr) == -1) {
        std::cerr << "Błąd timerfd_settime (stop): " << strerror(errno) << std::endl;
    }
}

void LTimer::onTimeout(std::function<void()> callback)
{
    m_callback = callback;
}

void LTimer::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLIN) {
        uint64_t expirations = 0;
        ssize_t s = read(m_timer_fd, &expirations, sizeof(expirations));

#warning 'wytlumacz mi dzialanie tego kodu ! '

        if (s == sizeof(expirations)) {
            if (m_callback)
                m_callback();
        }
    }
}
