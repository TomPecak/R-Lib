#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>

#include "LEventLoop.hpp"

class LTcpSocket;

/**
 * @brief TCP server wrapper analogous to Qt's QTcpServer.
 *
 * LTcpServer manages a listening socket. When a client connects,
 * it accepts the connection, creates an LTcpSocket instance, and
 * enqueues it for retrieval via nextPendingConnection().
 */
class LTcpServer : public LEpollHandler
{
public:
    LTcpServer();
    ~LTcpServer() override;

    bool listen(const std::string &address, uint16_t port);
    bool listen(uint16_t port);

    void close();

    bool hasPendingConnections() const;
    std::unique_ptr<LTcpSocket> nextPendingConnection();

    void onNewConnection(std::function<void()> callback);

    template<typename Object>
    void onNewConnection(Object *obj, void (Object::*method)())
    {
        m_newConnectionCallback = [obj, method]() { (obj->*method)(); };
    }

protected:
    void handleEpollEvent(uint32_t events) override;

private:
    int m_server_fd = -1;
    std::queue<std::unique_ptr<LTcpSocket>> m_pendingConnections;
    std::function<void()> m_newConnectionCallback;
};
