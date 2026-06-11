#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "LEpollHandler.hpp"

class LEventLoop;

class LTaskQueue : public LEpollHandler
{
public:
    LTaskQueue();
    ~LTaskQueue() override;

    LTaskQueue(const LTaskQueue &) = delete;
    LTaskQueue &operator=(const LTaskQueue &) = delete;

    bool attach(LEventLoop *loop);
    void detach();

    void postTask(std::function<void()> task);

protected:
    void handleEpollEvent(uint32_t events) override;

private:
    std::vector<std::function<void()>> takeAll();
    void flushTasks();

    LEventLoop *m_loop = nullptr;
    int m_event_fd = -1;
    std::mutex m_mutex;
    std::vector<std::function<void()>> m_pendingTasks;
};
