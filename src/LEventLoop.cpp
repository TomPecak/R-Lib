#include "LEventLoop.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

thread_local LEventLoop *t_currentLoop = nullptr;

LEventLoop::LEventLoop()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    t_currentLoop = this;
    m_threadId = std::this_thread::get_id();
    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1) {
        std::cerr << "LEventLoop epoll_create1 error: " << strerror(errno) << std::endl;
    }

    if (m_epoll_fd != -1 && !m_crossThreadQueue.attach(this)) {
        std::cerr << "LEventLoop task queue setup error: " << strerror(errno) << std::endl;
    }
}

LEventLoop::~LEventLoop()
{
    t_currentLoop = nullptr;
    m_crossThreadQueue.detach();
    if (m_epoll_fd != -1) {
        close(m_epoll_fd);
    }
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

bool LEventLoop::modifyHandler(int fd, uint32_t events, LEpollHandler *handler)
{
    struct epoll_event event;
    event.events = events;
    event.data.ptr = handler;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
        std::cerr << "LEventLoop epoll_ctl MOD error: " << strerror(errno) << std::endl;
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
        int timeout = m_localTasks.empty() ? -1 : 0;
        int n = epoll_wait(m_epoll_fd, events, MAX_EVENTS, timeout);

        if (n == -1) {
            if (errno == EINTR)
                continue; // Ignore system interrupts
            std::cerr << "LEventLoop epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }

        flushLocalTasks();

        for (int i = 0; i < n; ++i) {
            auto *handler = static_cast<LEpollHandler *>(events[i].data.ptr);
            if (handler) {
                handler->handleEpollEvent(events[i].events);
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
    if (std::this_thread::get_id() == m_threadId) {
        m_localTasks.push_back(std::move(task));
    } else {
        m_crossThreadQueue.postTask(std::move(task));
    }
}

void LEventLoop::flushLocalTasks()
{
    std::vector<std::function<void()>> tasksToRun;
    tasksToRun.swap(m_localTasks);

    for (auto &task : tasksToRun) {
        if (task) {
            task();
        }
    }
}
