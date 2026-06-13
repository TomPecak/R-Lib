#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>

#include "LEventLoop.hpp"
#include "LTcpSocket.hpp"

/**
 * @brief TCP server wrapper analogous to Qt's QTcpServer.
 *
 * LTcpServer manages a listening socket. When a client connects,
 * it accepts the connection and enqueues the descriptor for later
 * socket construction via nextPendingConnection().
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

    template<typename SocketType = LTcpSocket>
    std::unique_ptr<SocketType> nextPendingConnection()
    {
        if (m_pendingConnections.empty()) {
            return nullptr;
        }

        int fd = m_pendingConnections.front();
        m_pendingConnections.pop();
        return std::make_unique<SocketType>(fd);
    }

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
    std::queue<int> m_pendingConnections;
    std::function<void()> m_newConnectionCallback;
};
