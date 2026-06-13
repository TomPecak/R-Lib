#include "LUdpSocket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

LUdpSocket::LUdpSocket()
    : m_socket_fd(-1)
    , m_state(UnconnectedState)
    , m_error(UnknownSocketError)
    , m_peerPort(0)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    // Create a UDP socket (IPv4).
    // The SOCK_NONBLOCK (non-blocking) and SOCK_CLOEXEC (close-on-exec for fork safety) flags
    // are crucial for performance in an epoll-based architecture.
    m_socket_fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (m_socket_fd == -1) {
        setError(SocketResourceError, strerror(errno));
        return;
    }

    // Immediately register the descriptor in the event loop (listen for read events)
    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->registerHandler(m_socket_fd, EPOLLIN, this);
    } else {
        std::cerr << "LUdpSocket Warning: No LEventLoop in current thread!" << std::endl;
    }
}

LUdpSocket::~LUdpSocket()
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
    m_errorCallback = nullptr;
    m_stateCallback = nullptr;

    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

bool LUdpSocket::bind(uint16_t port, BindFlag mode)
{
    // 0.0.0.0 means listening on all available network interfaces
    return bind("0.0.0.0", port, mode);
}

bool LUdpSocket::bind(const std::string &address, uint16_t port, BindFlag mode)
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
    addr.sin_port = htons(port); // Convert to network byte order (Big Endian)

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

    setState(BoundState);
    return true;
}

void LUdpSocket::connectToHost(const std::string &hostName, uint16_t port)
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

    // connect() on UDP does not send any network packets!
    // It merely tells the kernel: "From now on, the default destination address is X".
    if (::connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        setError(NetworkError, strerror(errno));
        return;
    }

    m_peerAddress = hostName;
    m_peerPort = port;
    setState(ConnectedState);
}

void LUdpSocket::disconnectFromHost()
{
    if (m_socket_fd == -1)
        return;

    // In Linux, disconnecting a "connected" UDP socket is done by specifying the AF_UNSPEC family
    struct sockaddr_in unspec;
    std::memset(&unspec, 0, sizeof(unspec));
    unspec.sin_family = AF_UNSPEC;
    ::connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&unspec), sizeof(unspec));

    m_peerAddress.clear();
    m_peerPort = 0;

    // Change the state back to Unconnected (or Bound, if it was bound)
    setState(UnconnectedState);
}

void LUdpSocket::abort()
{
    disconnectFromHost();
}

bool LUdpSocket::hasPendingDatagrams() const
{
    return pendingDatagramSize() > 0;
}

int64_t LUdpSocket::pendingDatagramSize() const
{
    if (m_socket_fd == -1)
        return -1;

    int bytes_available = 0;
    // ioctl with the FIONREAD command asks the kernel how many bytes are waiting in the read queue.
    // It returns the size of the first datagram. Fast and cheap system call.
    if (ioctl(m_socket_fd, FIONREAD, &bytes_available) == -1) {
        return -1;
    }
    return bytes_available;
}

int64_t LUdpSocket::readDatagram(char *data, int64_t maxSize, std::string *address, uint16_t *port)
{
    if (m_socket_fd == -1)
        return -1;

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    // Read data and simultaneously retrieve sender information
    ssize_t ret = ::recvfrom(m_socket_fd,
                             data,
                             maxSize,
                             0,
                             reinterpret_cast<struct sockaddr *>(&sender),
                             &sender_len);

    if (ret > 0) {
        if (address) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender.sin_addr), ip_str, INET_ADDRSTRLEN);
            *address = std::string(ip_str);
        }
        if (port) {
            *port = ntohs(sender.sin_port);
        }
    } else if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        setError(NetworkError, strerror(errno));
    }

    return ret;
}

std::vector<uint8_t> LUdpSocket::receiveDatagram(std::string *address, uint16_t *port)
{
    int64_t size = pendingDatagramSize();
    if (size <= 0)
        return {};

    std::vector<uint8_t> buffer(size);
    int64_t read_bytes = readDatagram(reinterpret_cast<char *>(buffer.data()), size, address, port);

    if ((read_bytes < size) && (read_bytes > 0)) {
        buffer.resize(read_bytes); // Safeguard in case fewer bytes were read than expected
    } else if (read_bytes <= 0) {
        return {};
    }

    return buffer;
}

int64_t LUdpSocket::writeDatagram(const char *data,
                                  int64_t size,
                                  const std::string &address,
                                  uint16_t port)
{
    if (m_socket_fd == -1)
        return -1;

    struct sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);

    if (inet_pton(AF_INET, address.c_str(), &dest.sin_addr) <= 0) {
        setError(NetworkError, "Invalid destination IP");
        return -1;
    }

    ssize_t ret = ::sendto(m_socket_fd,
                           data,
                           size,
                           0,
                           reinterpret_cast<struct sockaddr *>(&dest),
                           sizeof(dest));

    if (ret > 0 && m_bytesWrittenCallback) {
        m_bytesWrittenCallback(ret);
    } else if (ret == -1) {
        if (errno == EMSGSIZE)
            setError(DatagramTooLargeError, strerror(errno));
        else
            setError(NetworkError, strerror(errno));
    }

    return ret;
}

int64_t LUdpSocket::writeDatagram(const std::vector<uint8_t> &datagram,
                                  const std::string &address,
                                  uint16_t port)
{
    return writeDatagram(reinterpret_cast<const char *>(datagram.data()),
                         datagram.size(),
                         address,
                         port);
}

int64_t LUdpSocket::write(const char *data, int64_t size)
{
    if (m_socket_fd == -1 || m_state != ConnectedState)
        return -1;

    // Because we called connect(), we can use the standard, super-fast send()
    ssize_t ret = ::send(m_socket_fd, data, size, 0);

    if ((ret > 0) && m_bytesWrittenCallback) {
        m_bytesWrittenCallback(ret);
    } else if (ret == -1) {
        setError(NetworkError, strerror(errno));
    }

    return ret;
}

LUdpSocket::SocketState LUdpSocket::state() const
{
    return m_state;
}
LUdpSocket::SocketError LUdpSocket::error() const
{
    return m_error;
}
std::string LUdpSocket::errorString() const
{
    return m_errorString;
}

void LUdpSocket::onReadyRead(std::function<void()> callback)
{
    m_readyReadCallback = callback;
}
void LUdpSocket::onBytesWritten(std::function<void(int64_t)> callback)
{
    m_bytesWrittenCallback = callback;
}
void LUdpSocket::onErrorOccurred(std::function<void(SocketError)> callback)
{
    m_errorCallback = callback;
}
void LUdpSocket::onStateChanged(std::function<void(SocketState)> callback)
{
    m_stateCallback = callback;
}

void LUdpSocket::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLIN) {
        // We woke up because the kernel informs us that there is data in the receive buffer
        if (m_readyReadCallback) {
            m_readyReadCallback();
        }
    }
}

void LUdpSocket::setError(SocketError error, const std::string &errorString)
{
    m_error = error;
    m_errorString = errorString;
    if (m_errorCallback) {
        m_errorCallback(m_error);
    }
}

void LUdpSocket::setState(SocketState state)
{
    if (m_state != state) {
        m_state = state;
        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}