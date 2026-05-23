#include "LEventLoop.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

thread_local LEventLoop *t_currentLoop = nullptr;

LEventLoop::LEventLoop()
    : m_running(false)
{
    t_currentLoop = this;
    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1) {
        std::cerr << "Błąd epoll_create1: " << strerror(errno) << std::endl;
    }
}

LEventLoop::~LEventLoop()
{
    t_currentLoop = nullptr;
    close(m_epoll_fd);
    m_epoll_fd = -1;
}

bool LEventLoop::registerHandler(int fd, uint32_t events, LEpollHandler *handler)
{
    struct epoll_event event;
    event.events = events;
    // MAGIA: Zamiast trzymać (int fd), trzymamy wskaźnik do obiektu (np. LTimer)!
    event.data.ptr = handler;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::cerr << "Błąd epoll_ctl ADD: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void LEventLoop::unregisterHandler(int fd)
{
    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
}

int LEventLoop::exec()
{
    if (m_epoll_fd == -1)
        return -1;

    m_running = true;
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    while (m_running) {
        int n = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);

        if (n == -1) {
            if (errno == EINTR)
                continue; // Zignoruj przerwania od systemu
            std::cerr << "Błąd epoll_wait: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < n; ++i) {
            // Wyciągamy wskaźnik do naszego obiektu C++
            auto *handler = static_cast<LEpollHandler *>(events[i].data.ptr);
            if (handler) {
                // Wywołujemy kod konkretnego urządzenia
                handler->handleEpollEvent(events[i].events);
            }
        }
    }
    return 0;
}

LEventLoop *LEventLoop::current()
{
    return t_currentLoop;
}
