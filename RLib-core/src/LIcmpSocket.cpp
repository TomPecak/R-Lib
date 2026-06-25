#include "LIcmpSocket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

namespace {

struct IcmpEchoHeader {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

uint16_t calculateChecksum(const uint16_t *data, size_t length)
{
    uint32_t sum = 0;

    while (length > 1) {
        sum += *data++;
        length -= sizeof(uint16_t);
    }

    if (length == 1) {
        sum += *reinterpret_cast<const uint8_t *>(data);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

}

LIcmpSocket::LIcmpSocket()
    : m_socket_fd(-1)
    , m_state(UnconnectedState)
    , m_error(UnknownSocketError)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    m_socket_fd = ::socket(AF_INET, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_ICMP);

    if (m_socket_fd == -1) {
        if (errno == EPERM || errno == EACCES) {
            setError(SocketAccessError,
                     "Permission denied creating raw ICMP socket (requires root / CAP_NET_RAW)");
        } else {
            setError(SocketResourceError, strerror(errno));
        }
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->registerHandler(m_socket_fd, EPOLLIN, this);
    } else {
        std::cerr << "LIcmpSocket Warning: No LEventLoop in current thread!" << std::endl;
    }
}

LIcmpSocket::~LIcmpSocket()
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

bool LIcmpSocket::bind(const std::string &address)
{
    if (m_socket_fd == -1)
        return false;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        setError(NetworkError, "Invalid IP address format: " + address);
        return false;
    }

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

void LIcmpSocket::connectToHost(const std::string &hostName)
{
    if (m_socket_fd == -1)
        return;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    if (inet_pton(AF_INET, hostName.c_str(), &addr.sin_addr) <= 0) {
        setError(NetworkError, "Invalid IP address: " + hostName);
        return;
    }

    if (::connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        setError(NetworkError, strerror(errno));
        return;
    }

    m_peerAddress = hostName;
    setState(ConnectedState);
}

void LIcmpSocket::disconnectFromHost()
{
    if (m_socket_fd == -1)
        return;

    struct sockaddr_in unspec;
    std::memset(&unspec, 0, sizeof(unspec));
    unspec.sin_family = AF_UNSPEC;
    ::connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&unspec), sizeof(unspec));

    m_peerAddress.clear();
    setState(UnconnectedState);
}

void LIcmpSocket::abort()
{
    disconnectFromHost();
}

bool LIcmpSocket::hasPendingDatagrams() const
{
    return pendingDatagramSize() > 0;
}

int64_t LIcmpSocket::pendingDatagramSize() const
{
    if (m_socket_fd == -1)
        return -1;

    int bytes_available = 0;
    if (ioctl(m_socket_fd, FIONREAD, &bytes_available) == -1) {
        return -1;
    }
    return bytes_available;
}

int64_t LIcmpSocket::readDatagram(char *data, int64_t maxSize, std::string *address)
{
    if (m_socket_fd == -1)
        return -1;

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

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
    } else if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        setError(NetworkError, strerror(errno));
    }

    return ret;
}

std::vector<uint8_t> LIcmpSocket::receiveDatagram(std::string *address)
{
    int64_t size = pendingDatagramSize();
    if (size <= 0)
        return {};

    std::vector<uint8_t> buffer(size);
    int64_t read_bytes = readDatagram(reinterpret_cast<char *>(buffer.data()), size, address);

    if ((read_bytes < size) && (read_bytes > 0)) {
        buffer.resize(read_bytes);
    } else if (read_bytes <= 0) {
        return {};
    }

    return buffer;
}

int64_t LIcmpSocket::ping(const std::string &address,
                          uint16_t id,
                          uint16_t sequence,
                          const std::vector<uint8_t> &payload)
{
    const size_t packetSize = sizeof(IcmpEchoHeader) + payload.size();
    std::vector<uint8_t> packet(packetSize, 0);

    IcmpEchoHeader *header = reinterpret_cast<IcmpEchoHeader *>(packet.data());
    header->type = ICMP_ECHO;
    header->code = 0;
    header->id = htons(id);
    header->sequence = htons(sequence);

    if (!payload.empty()) {
        std::memcpy(packet.data() + sizeof(IcmpEchoHeader), payload.data(), payload.size());
    }

    header->checksum = 0;
    header->checksum = calculateChecksum(reinterpret_cast<const uint16_t *>(packet.data()),
                                         packet.size());

    return writeDatagram(reinterpret_cast<const char *>(packet.data()),
                         static_cast<int64_t>(packet.size()),
                         address);
}

bool LIcmpSocket::readEchoReply(EchoReply *reply)
{
    if (!reply)
        return false;

    while (hasPendingDatagrams()) {
        std::string senderAddress;
        std::vector<uint8_t> datagram = receiveDatagram(&senderAddress);
        if (datagram.empty())
            return false;

        if (parseEchoReply(datagram, reply)) {
            reply->address = senderAddress;
            return true;
        }
    }

    return false;
}

int64_t LIcmpSocket::write(const char *data, int64_t size)
{
    if (m_socket_fd == -1 || m_state != ConnectedState)
        return -1;

    ssize_t ret = ::send(m_socket_fd, data, size, 0);

    if ((ret > 0) && m_bytesWrittenCallback) {
        m_bytesWrittenCallback(ret);
    } else if (ret == -1) {
        if (errno == EMSGSIZE)
            setError(DatagramTooLargeError, strerror(errno));
        else
            setError(NetworkError, strerror(errno));
    }

    return ret;
}

int64_t LIcmpSocket::writeDatagram(const char *data,
                                   int64_t size,
                                   const std::string &address)
{
    if (m_socket_fd == -1)
        return -1;

    struct sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;

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

LIcmpSocket::SocketState LIcmpSocket::state() const
{
    return m_state;
}

LIcmpSocket::SocketError LIcmpSocket::error() const
{
    return m_error;
}

std::string LIcmpSocket::errorString() const
{
    return m_errorString;
}

void LIcmpSocket::onReadyRead(std::function<void()> callback)
{
    m_readyReadCallback = callback;
}

void LIcmpSocket::onBytesWritten(std::function<void(int64_t)> callback)
{
    m_bytesWrittenCallback = callback;
}

void LIcmpSocket::onErrorOccurred(std::function<void(SocketError)> callback)
{
    m_errorCallback = callback;
}

void LIcmpSocket::onStateChanged(std::function<void(SocketState)> callback)
{
    m_stateCallback = callback;
}

void LIcmpSocket::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLIN) {
        if (m_readyReadCallback) {
            m_readyReadCallback();
        }
    }
}

void LIcmpSocket::setError(SocketError error, const std::string &errorString)
{
    m_error = error;
    m_errorString = errorString;
    if (m_errorCallback) {
        m_errorCallback(m_error);
    }
}

void LIcmpSocket::setState(SocketState state)
{
    if (m_state != state) {
        m_state = state;
        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

bool LIcmpSocket::parseEchoReply(const std::vector<uint8_t> &datagram, EchoReply *reply) const
{
    if (datagram.size() < sizeof(struct ip))
        return false;

    const struct ip *ipHeader = reinterpret_cast<const struct ip *>(datagram.data());
    const size_t ipHeaderLen = ipHeader->ip_hl * 4;

    if (datagram.size() < ipHeaderLen + sizeof(IcmpEchoHeader))
        return false;

    const IcmpEchoHeader *icmpHeader =
        reinterpret_cast<const IcmpEchoHeader *>(datagram.data() + ipHeaderLen);

    if (icmpHeader->type != ICMP_ECHOREPLY || icmpHeader->code != 0)
        return false;

    const size_t payloadOffset = ipHeaderLen + sizeof(IcmpEchoHeader);
    if (datagram.size() > payloadOffset) {
        reply->payload.assign(datagram.begin() + payloadOffset, datagram.end());
    } else {
        reply->payload.clear();
    }

    reply->id = ntohs(icmpHeader->id);
    reply->sequence = ntohs(icmpHeader->sequence);
    return true;
}
