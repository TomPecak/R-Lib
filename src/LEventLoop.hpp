#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

class LEpollHandler
{
public:
    virtual ~LEpollHandler() = default;
    virtual void handleEpollEvent(uint32_t events) = 0;
};

class LEventLoop
{
public:
    LEventLoop();
    ~LEventLoop();

    bool registerHandler(int fd, uint32_t events, LEpollHandler *handler);
    void unregisterHandler(int fd);

    int exec();

    static void quit();

    static LEventLoop *current();

    void postTask(std::function<void()> task);

private:
    void flushTasks();

    int m_epoll_fd;
    bool m_running;
    int m_event_fd = -1;
    std::vector<std::function<void()>> m_pendingTasks;
    std::mutex m_taskMutex;
};
