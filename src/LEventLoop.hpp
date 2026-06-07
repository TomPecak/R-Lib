#pragma once
#include <functional>
#include <thread>
#include <vector>

#include "LEpollHandler.hpp"
#include "LTaskQueue.hpp"

class LEventLoop
{
public:
    LEventLoop();
    ~LEventLoop();

    bool registerHandler(int fd, uint32_t events, LEpollHandler *handler);
    bool modifyHandler(int fd, uint32_t events, LEpollHandler *handler);
    void unregisterHandler(int fd);

    int exec();

    static void quit();

    static LEventLoop *current();

    /**
     * @brief Post a task to be executed on the event loop.
     *
     * This method is thread-safe and can be safely called from other threads.
     * If called from the event loop's own thread, the task is executed locally
     * without crossing thread boundaries or acquiring queue locks.
     */
    void postTask(std::function<void()> task);

private:
    void flushLocalTasks();

    int m_epoll_fd = -1;
    bool m_running = false;
    LTaskQueue m_crossThreadQueue;

    std::thread::id m_threadId;
    std::vector<std::function<void()>> m_localTasks;
};
