#include "LTcpSocket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

LTcpSocket::LTcpSocket()
    : m_socket_fd(-1)
    , m_state(UnconnectedState)
    , m_error(UnknownSocketError)
    , m_localPort(0)
    , m_peerPort(0)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    // Create a TCP socket (IPv4).
    // The SOCK_NONBLOCK and SOCK_CLOEXEC flags are crucial for performance
    // in an epoll-based architecture.
    m_socket_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (m_socket_fd == -1) {
        setError(SocketResourceError, strerror(errno));
        return;
    }

    // Immediately register the descriptor in the event loop (listen for read events)
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
    , m_localPort(0)
    , m_peerPort(0)
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

    m_readyReadCallback = nullptr;
    m_bytesWrittenCallback = nullptr;
    m_connectedCallback = nullptr;
    m_disconnectedCallback = nullptr;
    m_errorCallback = nullptr;
    m_stateCallback = nullptr;
    m_newConnectionCallback = nullptr;

    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

bool LTcpSocket::bind(uint16_t port, BindFlag mode)
{
    // 0.0.0.0 means listening on all available network interfaces
    return bind("0.0.0.0", port, mode);
}

bool LTcpSocket::bind(const std::string &address, uint16_t port, BindFlag mode)
{
    if (m_socket_fd == -1)
        return false;

    // Handle binding flags (ShareAddress / ReuseAddressHint -> SO_REUSEADDR)
    if (mode == ShareAddress || mode == ReuseAddressHint) {
        int opt = 1;
        if (setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            setError(SocketAccessError,
                     "Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        }
    }

    // Configure the IPv4 address structure
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        setError(NetworkError, "Invalid IP address format: " + address);
        return false;
    }

    // The actual bind system call
    if (::bind(m_socket_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        if (errno == EADDRINUSE) {
            setError(AddressInUseError, strerror(errno));
        } else {
            setError(SocketAccessError, strerror(errno));
        }
        return false;
    }

    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    if (::getsockname(m_socket_fd, reinterpret_cast<struct sockaddr *>(&local_addr), &len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        m_localAddress = std::string(ip_str);
        m_localPort = ntohs(local_addr.sin_port);
    }

    setState(BoundState);
    return true;
}

bool LTcpSocket::listen(int backlog)
{
    if (m_socket_fd == -1)
        return false;

    if (::listen(m_socket_fd, backlog) == -1) {
        setError(SocketAccessError, strerror(errno));
        return false;
    }

    setState(ListeningState);
    return true;
}

std::unique_ptr<LTcpSocket> LTcpSocket::accept()
{
    if (m_socket_fd == -1 || m_state != ListeningState)
        return nullptr;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept4(m_socket_fd,
                              reinterpret_cast<struct sockaddr *>(&client_addr),
                              &client_len,
                              SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            setError(NetworkError, strerror(errno));
        }
        return nullptr;
    }

    return std::unique_ptr<LTcpSocket>(new LTcpSocket(client_fd));
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

    // connect() on a non-blocking TCP socket returns immediately.
    // We wait for the EPOLLOUT event to confirm the connection.
    int ret = ::connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            m_peerAddress = hostName;
            m_peerPort = port;
            setState(ConnectingState);

            LEventLoop *loop = LEventLoop::current();
            if (loop) {
                loop->unregisterHandler(m_socket_fd);
                loop->registerHandler(m_socket_fd, EPOLLIN | EPOLLOUT, this);
            }
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
    if (m_socket_fd == -1 || m_state != ConnectedState)
        return -1;

    ssize_t ret = ::recv(m_socket_fd, data, maxSize, 0);

    if (ret == 0) {
        setError(RemoteHostClosedError, "Remote host closed the connection");
        setState(ClosingState);
        if (m_disconnectedCallback) {
            m_disconnectedCallback();
        }
        return 0;
    } else if (ret == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            setError(NetworkError, strerror(errno));
        }
        return -1;
    }

    return ret;
}

std::vector<uint8_t> LTcpSocket::readAll()
{
    std::vector<uint8_t> buffer;
    char temp[4096];

    while (true) {
        int64_t bytes = read(temp, sizeof(temp));
        if (bytes > 0) {
            buffer.insert(buffer.end(), temp, temp + bytes);
        } else {
            break;
        }
    }

    return buffer;
}

int64_t LTcpSocket::write(const char *data, int64_t size)
{
    if (m_socket_fd == -1 || m_state != ConnectedState)
        return -1;

    ssize_t ret = ::send(m_socket_fd, data, size, MSG_NOSIGNAL);

    if (ret > 0 && m_bytesWrittenCallback) {
        m_bytesWrittenCallback(ret);
    } else if (ret == -1) {
        if (errno == EPIPE || errno == ECONNRESET) {
            setError(RemoteHostClosedError, strerror(errno));
            setState(ClosingState);
            if (m_disconnectedCallback) {
                m_disconnectedCallback();
            }
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            setError(NetworkError, strerror(errno));
        }
    }

    return ret;
}

int64_t LTcpSocket::write(const std::vector<uint8_t> &data)
{
    return write(reinterpret_cast<const char *>(data.data()), data.size());
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
void LTcpSocket::onNewConnection(std::function<void()> callback)
{
    m_newConnectionCallback = callback;
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
        return;
    }

    if (events & EPOLLOUT) {
        if (m_state == ConnectingState) {
            setState(ConnectedState);
            if (m_connectedCallback) {
                m_connectedCallback();
            }
            LEventLoop *loop = LEventLoop::current();
            if (loop) {
                loop->unregisterHandler(m_socket_fd);
                loop->registerHandler(m_socket_fd, EPOLLIN, this);
            }
        }
    }

    if (events & EPOLLIN) {
        if (m_state == ListeningState) {
            if (m_newConnectionCallback) {
                m_newConnectionCallback();
            }
        } else if (m_state == ConnectedState || m_state == ClosingState) {
            if (m_readyReadCallback) {
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
