#include "LEventLoop.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

thread_local LEventLoop *t_currentLoop = nullptr;

LEventLoop::LEventLoop()
    : m_running(false)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    t_currentLoop = this;
    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1) {
        std::cerr << "LEventLoop epoll_create1 error: " << strerror(errno) << std::endl;
    }

m_event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_event_fd != -1) {
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.ptr = nullptr;
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_event_fd, &event);
    }
}

LEventLoop::~LEventLoop()
{
    t_currentLoop = nullptr;
    if (m_event_fd != -1) {
        close(m_event_fd);
        m_event_fd = -1;
    }
    close(m_epoll_fd);
    m_epoll_fd = -1;
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

bool LEventLoop::registerHandler(int fd, uint32_t events, LEpollHandler *handler)
{
    struct epoll_event event;
    event.events = events;
    event.data.ptr = handler;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::cerr << "LEventLoop epoll_ctl ADD error: " << strerror(errno) << std::endl;
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
                continue; // Ignore system interrupts
            std::cerr << "LEventLoop epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < n; ++i) {
            auto *handler = static_cast<LEpollHandler *>(events[i].data.ptr);
            if (handler) {
                handler->handleEpollEvent(events[i].events);
            } else {
                uint64_t val;
                (void)::read(m_event_fd, &val, sizeof(val));
                flushTasks();
            }
        }
    }
    return 0;
}

void LEventLoop::quit()
{
    if (t_currentLoop) {
        t_currentLoop->m_running = false;
    }
}

LEventLoop *LEventLoop::current()
{
    return t_currentLoop;
}

void LEventLoop::postTask(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_pendingTasks.push_back(std::move(task));
    }
    if (m_event_fd != -1) {
        uint64_t val = 1;
        (void)::write(m_event_fd, &val, sizeof(val));
    }
}

void LEventLoop::flushTasks()
{
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        tasks.swap(m_pendingTasks);
    }
    for (auto &task : tasks) {
        if (task) {
            task();
        }
    }
}
