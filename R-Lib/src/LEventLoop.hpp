#pragma once
#include <cstdint>

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

    static LEventLoop *current();

private:
    int m_epoll_fd;
    bool m_running;
};
