#include "LTaskQueue.hpp"

#include "LEventLoop.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cstdint>

LTaskQueue::LTaskQueue()
{
}

LTaskQueue::~LTaskQueue()
{
    detach();
}

bool LTaskQueue::attach(LEventLoop *loop)
{
    detach();

    if (!loop) {
        return false;
    }

    m_event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_event_fd == -1) {
        return false;
    }

    if (!loop->registerHandler(m_event_fd, EPOLLIN, this)) {
        ::close(m_event_fd);
        m_event_fd = -1;
        return false;
    }

    m_loop = loop;
    return true;
}

void LTaskQueue::detach()
{
    if (m_event_fd != -1) {
        if (m_loop) {
            m_loop->unregisterHandler(m_event_fd);
        }
        ::close(m_event_fd);
        m_event_fd = -1;
    }

    m_loop = nullptr;
}

void LTaskQueue::postTask(std::function<void()> task)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingTasks.push_back(std::move(task));

    if (m_event_fd != -1) {
        uint64_t val = 1;
        (void)::write(m_event_fd, &val, sizeof(val));
    }
}

void LTaskQueue::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLIN) {
        uint64_t val;
        while (::read(m_event_fd, &val, sizeof(val)) > 0) {
        }
        flushTasks();
    }
}

std::vector<std::function<void()>> LTaskQueue::takeAll()
{
    std::vector<std::function<void()>> tasks;
    std::lock_guard<std::mutex> lock(m_mutex);
    tasks.swap(m_pendingTasks);
    return tasks;
}

void LTaskQueue::flushTasks()
{
    std::vector<std::function<void()>> tasks = takeAll();
    for (auto &task : tasks) {
        if (task) {
            task();
        }
    }
}
