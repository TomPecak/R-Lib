#pragma once
#include <functional>

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

    void postTask(std::function<void()> task);

private:
    int m_epoll_fd = -1;
    bool m_running = false;
    LTaskQueue m_taskQueue;
};
