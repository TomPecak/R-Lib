#include "LTcpSocket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

LTcpSocket::LTcpSocket()
    : m_state(UnconnectedState)
    , m_error(UnknownSocketError)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    m_socket_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK |
                                                  SOCK_CLOEXEC, 0);
    if (m_socket_fd == -1) {
        setError(SocketResourceError, strerror(errno));
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->registerHandler(m_socket_fd, EPOLLIN, this);
    } else {
        std::cerr << "LTcpSocket Warning: No LEventLoop in current thread!" << std::endl;
    }
}

LTcpSocket::LTcpSocket(int socket_fd)
    : m_socket_fd(socket_fd)
    , m_state(ConnectedState)
    , m_error(UnknownSocketError)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    if (::getpeername(m_socket_fd, reinterpret_cast<struct sockaddr *>(&peer_addr), &peer_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        m_peerAddress = std::string(ip_str);
        m_peerPort = ntohs(peer_addr.sin_port);
    }

    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (::getsockname(m_socket_fd, reinterpret_cast<struct sockaddr *>(&local_addr), &local_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        m_localAddress = std::string(ip_str);
        m_localPort = ntohs(local_addr.sin_port);
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->registerHandler(m_socket_fd, EPOLLIN, this);
    } else {
        std::cerr << "LTcpSocket Warning: No LEventLoop in current thread!" << std::endl;
    }
}

LTcpSocket::~LTcpSocket()
{
    if (m_socket_fd != -1) {
        LEventLoop *loop = LEventLoop::current();
        if (loop) {
            loop->unregisterHandler(m_socket_fd);
        }
        ::close(m_socket_fd);
        m_socket_fd = -1;
    }

    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

void LTcpSocket::updateEpollInterest(uint32_t events)
{
    if (m_socket_fd == -1)
        return;

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->unregisterHandler(m_socket_fd);
        loop->registerHandler(m_socket_fd, events, this);
    }
}

void LTcpSocket::connectToHost(const std::string &hostName, uint16_t port)
{
    if (m_socket_fd == -1)
        return;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, hostName.c_str(), &addr.sin_addr) <= 0) {
        setError(NetworkError, "Invalid IP address: " + hostName);
        return;
    }

    int ret = ::connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            m_peerAddress = hostName;
            m_peerPort = port;
            setState(ConnectingState);
            updateEpollInterest(EPOLLOUT);
            return;
        } else if (errno == ECONNREFUSED) {
            setError(ConnectionRefusedError, strerror(errno));
        } else {
            setError(NetworkError, strerror(errno));
        }
        return;
    }

    m_peerAddress = hostName;
    m_peerPort = port;
    setState(ConnectedState);
    if (m_connectedCallback) {
        m_connectedCallback();
    }
}

void LTcpSocket::disconnectFromHost()
{
    if (m_socket_fd == -1)
        return;

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->unregisterHandler(m_socket_fd);
    }

    ::shutdown(m_socket_fd, SHUT_RDWR);
    ::close(m_socket_fd);
    m_socket_fd = -1;

    m_readBuffer.clear();
    m_writeBuffer.clear();

    m_peerAddress.clear();
    m_peerPort = 0;

    setState(UnconnectedState);
    if (m_disconnectedCallback) {
        m_disconnectedCallback();
    }
}

void LTcpSocket::abort()
{
    disconnectFromHost();
}

int64_t LTcpSocket::read(char *data, int64_t maxSize)
{
    if (maxSize <= 0 || m_readBuffer.empty())
        return 0;

    int64_t bytesToRead = std::min<int64_t>(maxSize, static_cast<int64_t>(m_readBuffer.size()));
    if (bytesToRead <= 0)
        return 0;

    std::copy(m_readBuffer.begin(), m_readBuffer.begin() + bytesToRead, data);
    m_readBuffer.erase(m_readBuffer.begin(), m_readBuffer.begin() + bytesToRead);

    if (!m_readBuffer.empty() && m_readyReadCallback) {
        LEventLoop *loop = LEventLoop::current();
        if (loop) {
           loop->postTask([this]() {
               if (!m_readBuffer.empty() && m_readyReadCallback) {
                   m_readyReadCallback();
               }
           });
        }
    }
    return bytesToRead;
}

std::vector<uint8_t> LTcpSocket::readAll()
{
    std::vector<uint8_t> data;
    data.swap(m_readBuffer);
    return data;
}

int64_t LTcpSocket::bytesAvailable() const
{
    return static_cast<int64_t>(m_readBuffer.size());
}

int64_t LTcpSocket::write(const char *data, int64_t size)
{
    if (m_socket_fd == -1 || m_state != ConnectedState)
        return -1;

    if (size <= 0)
        return 0;

    // if the transmit buffer is empty, attempt an immediate non-blocking send
    if (m_writeBuffer.empty()) {
        ssize_t ret = ::send(m_socket_fd, data, size, MSG_NOSIGNAL);
        if (ret > 0) {
            if (m_bytesWrittenCallback) {
                m_bytesWrittenCallback(ret);
            }

            if (ret < size) {
                // queue the remainder
                m_writeBuffer.insert(m_writeBuffer.end(), data + ret, data + size);
                updateEpollInterest(EPOLLIN | EPOLLOUT);
            }
            return size;
        } else if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // kernel buffer full, fall through to buffered path
            } else if (errno == EPIPE || errno == ECONNRESET) {
                setError(RemoteHostClosedError, strerror(errno));
                setState(ClosingState);
                if (m_disconnectedCallback) {
                    m_disconnectedCallback();
                }
                return -1;
            } else {
                setError(NetworkError, strerror(errno));
                return -1;
            }
        }
    }

    // buffer the data for asynchronous transmission
    m_writeBuffer.insert(m_writeBuffer.end(), data, data + size);
    updateEpollInterest(EPOLLIN | EPOLLOUT);

    return size;
}

int64_t LTcpSocket::write(const std::vector<uint8_t> &data)
{
    return write(reinterpret_cast<const char *>(data.data()), data.size());
}

void LTcpSocket::flushWriteBuffer()
{
    if (m_socket_fd == -1 || m_writeBuffer.empty())
        return;

    while (!m_writeBuffer.empty()) {
        ssize_t ret = ::send(m_socket_fd, m_writeBuffer.data(), m_writeBuffer.size(), MSG_NOSIGNAL);
        if (ret > 0) {
            if (m_bytesWrittenCallback) {
                m_bytesWrittenCallback(ret);
            }
            m_writeBuffer.erase(m_writeBuffer.begin(), m_writeBuffer.begin() + ret);
        } else if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // kernel buffer full; wait for the next EPOLLOUT event
                break;
            } else if (errno == EPIPE || errno == ECONNRESET) {
                setError(RemoteHostClosedError, strerror(errno));
                setState(ClosingState);
                if (m_disconnectedCallback) {
                    m_disconnectedCallback();
                }
                return;
            } else {
                setError(NetworkError, strerror(errno));
                return;
            }
        }
    }

    if (m_writeBuffer.empty()) {
        // remove EPOLLOUT to avoid busy-looping.
        updateEpollInterest(EPOLLIN);
    }
}

LTcpSocket::SocketState LTcpSocket::state() const
{
    return m_state;
}

LTcpSocket::SocketError LTcpSocket::error() const
{
    return m_error;
}

std::string LTcpSocket::errorString() const
{
    return m_errorString;
}

std::string LTcpSocket::localAddress() const
{
    return m_localAddress;
}

uint16_t LTcpSocket::localPort() const
{
    return m_localPort;
}

std::string LTcpSocket::peerAddress() const
{
    return m_peerAddress;
}

uint16_t LTcpSocket::peerPort() const
{
    return m_peerPort;
}

void LTcpSocket::onReadyRead(std::function<void()> callback)
{
    m_readyReadCallback = callback;
}

void LTcpSocket::onBytesWritten(std::function<void(int64_t)> callback)
{
    m_bytesWrittenCallback = callback;
}

void LTcpSocket::onConnected(std::function<void()> callback)
{
    m_connectedCallback = callback;
}

void LTcpSocket::onDisconnected(std::function<void()> callback)
{
    m_disconnectedCallback = callback;
}

void LTcpSocket::onErrorOccurred(std::function<void(SocketError)> callback)
{
    m_errorCallback = callback;
}

void LTcpSocket::onStateChanged(std::function<void(SocketState)> callback)
{
    m_stateCallback = callback;
}

void LTcpSocket::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLERR) {
        int error_code = 0;
        socklen_t len = sizeof(error_code);
        if (::getsockopt(m_socket_fd, SOL_SOCKET, SO_ERROR, &error_code, &len) == 0 && error_code != 0) {
            if (error_code == ECONNREFUSED) {
                setError(ConnectionRefusedError, strerror(error_code));
            } else {
                setError(NetworkError, strerror(error_code));
            }
        }
        if (m_state == ConnectingState) {
            setState(UnconnectedState);
        }

        if (m_socket_fd != -1) {
            LEventLoop *loop = LEventLoop::current();

            if (loop) {
                loop->unregisterHandler(m_socket_fd);
            }
            ::close(m_socket_fd);
            m_socket_fd = -1;
        }

        if (m_disconnectedCallback) {
            m_disconnectedCallback();
        }
        return;
    }

    if (events & EPOLLOUT) {
        if (m_state == ConnectingState) {
            setState(ConnectedState);
            if (m_connectedCallback) {
                m_connectedCallback();
            }
            if(m_writeBuffer.empty()) {
                updateEpollInterest(EPOLLIN);
            } else {
                updateEpollInterest(EPOLLIN | EPOLLOUT);
            }
        } else if (m_state == ConnectedState) {
            flushWriteBuffer();
        }
    }

    if (events & EPOLLIN) {
        if (m_state == ConnectedState ||
            m_state == ClosingState) {
            char temp[4096];
            while (true) {
                ssize_t ret = ::recv(m_socket_fd, temp, sizeof(temp), 0);
                if (ret > 0) {
                    m_readBuffer.insert(m_readBuffer.end(), temp, temp + ret);
                } else if (ret == 0) {
                    // peer performed an orderly shutdown
                    setError(RemoteHostClosedError, "Remote host closed the connection");
                    setState(ClosingState);
                    if (m_disconnectedCallback) {
                        m_disconnectedCallback();
                    }
                    break;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    setError(NetworkError, strerror(errno));
                    break;
                }
            }

            if (!m_readBuffer.empty() && m_readyReadCallback) {
                m_readyReadCallback();
            }
        }
    }

    if (events & EPOLLHUP) {
        setError(RemoteHostClosedError, "Connection hung up");
        setState(ClosingState);
        if (m_disconnectedCallback) {
            m_disconnectedCallback();
        }
    }
}

void LTcpSocket::setError(SocketError error, const std::string &errorString)
{
    m_error = error;
    m_errorString = errorString;
    if (m_errorCallback) {
        m_errorCallback(m_error);
    }
}

void LTcpSocket::setState(SocketState state)
{
    if (m_state != state) {
        m_state = state;
        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}
