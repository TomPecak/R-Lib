#include "LTcpServer.hpp"
#include "LTcpSocket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

LTcpServer::LTcpServer()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

LTcpServer::~LTcpServer()
{
    close();
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

bool LTcpServer::listen(uint16_t port)
{
    return listen("0.0.0.0", port);
}

bool LTcpServer::listen(const std::string &address, uint16_t port)
{
    if (m_server_fd != -1)
        return false;

    m_server_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (m_server_fd == -1) {
        std::cerr << "LTcpServer::listen socket error: " << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "LTcpServer::listen setsockopt error: " << strerror(errno) << std::endl;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "LTcpServer::listen invalid address: " << address << std::endl;
        ::close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    if (::bind(m_server_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        std::cerr << "LTcpServer::listen bind error: " << strerror(errno) << std::endl;
        ::close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    if (::listen(m_server_fd, 128) == -1) {
        std::cerr << "LTcpServer::listen listen error: " << strerror(errno) << std::endl;
        ::close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        if (!loop->registerHandler(m_server_fd, EPOLLIN, this)) {
            ::close(m_server_fd);
            m_server_fd = -1;
            return false;
        }
    } else {
        std::cerr << "LTcpServer Warning: No LEventLoop in current thread!" << std::endl;
    }

    return true;
}

void LTcpServer::close()
{
    while (!m_pendingConnections.empty()) {
        m_pendingConnections.pop();
    }

    if (m_server_fd != -1) {
        LEventLoop *loop = LEventLoop::current();
        if (loop) {
            loop->unregisterHandler(m_server_fd);
        }
        ::close(m_server_fd);
        m_server_fd = -1;
    }
}

bool LTcpServer::hasPendingConnections() const
{
    return !m_pendingConnections.empty();
}

std::unique_ptr<LTcpSocket> LTcpServer::nextPendingConnection()
{
    if (m_pendingConnections.empty())
        return nullptr;

    auto socket = std::move(m_pendingConnections.front());
    m_pendingConnections.pop();
    return socket;
}

void LTcpServer::onNewConnection(std::function<void()> callback)
{
    m_newConnectionCallback = callback;
}

void LTcpServer::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLIN) {
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept4(m_server_fd,
                                      reinterpret_cast<struct sockaddr *>(&client_addr),
                                      &client_len,
                                      SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                std::cerr << "LTcpServer::handleEpollEvent accept4 error: " << strerror(errno) << std::endl;
                break;
            }

            auto socket = std::unique_ptr<LTcpSocket>(new LTcpSocket(client_fd));
            m_pendingConnections.push(std::move(socket));

            if (m_newConnectionCallback) {
                m_newConnectionCallback();
            }
        }
    }

    if (events & EPOLLERR) {
        int error_code = 0;
        socklen_t len = sizeof(error_code);
        if (::getsockopt(m_server_fd, SOL_SOCKET, SO_ERROR, &error_code, &len) == 0 && error_code != 0) {
            std::cerr << "LTcpServer::handleEpollEvent EPOLLERR: " << strerror(error_code) << std::endl;
        }
    }
}
