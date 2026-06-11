#include "LNativeTcpSocket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

LNativeTcpSocket::LNativeTcpSocket()
{
}

LNativeTcpSocket::LNativeTcpSocket(int adoptedFd)
    : m_fd(adoptedFd)
{
}

LNativeTcpSocket::~LNativeTcpSocket()
{
    closeFd();
}

bool LNativeTcpSocket::open()
{
    closeFd();
    m_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (m_fd == -1) {
        recordErrno();
        return false;
    }
    return true;
}

bool LNativeTcpSocket::connectIPv4(const std::string &host, uint16_t port)
{
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        m_lastErrno = EINVAL;
        return false;
    }

    if (::connect(m_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        recordErrno();
        return false;
    }
    return true;
}

ssize_t LNativeTcpSocket::recvSome(void *buffer, size_t size, int flags)
{
    ssize_t ret = ::recv(m_fd, buffer, size, flags);
    if (ret == -1) {
        recordErrno();
    }
    return ret;
}

ssize_t LNativeTcpSocket::sendSome(const void *buffer, size_t size, int flags)
{
    ssize_t ret = ::send(m_fd, buffer, size, flags);
    if (ret == -1) {
        recordErrno();
    }
    return ret;
}

bool LNativeTcpSocket::shutdownReadWrite()
{
    if (m_fd == -1) {
        return true;
    }
    if (::shutdown(m_fd, SHUT_RDWR) == -1) {
        recordErrno();
        return false;
    }
    return true;
}

void LNativeTcpSocket::closeFd()
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool LNativeTcpSocket::loadLocalEndpoint(std::string *address, uint16_t *port) const
{
    if (m_fd == -1) {
        return false;
    }

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (::getsockname(m_fd, reinterpret_cast<struct sockaddr *>(&addr), &len) == -1) {
        return false;
    }

    char ip[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == nullptr) {
        return false;
    }

    if (address) {
        *address = ip;
    }
    if (port) {
        *port = ntohs(addr.sin_port);
    }
    return true;
}

bool LNativeTcpSocket::loadPeerEndpoint(std::string *address, uint16_t *port) const
{
    if (m_fd == -1) {
        return false;
    }

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (::getpeername(m_fd, reinterpret_cast<struct sockaddr *>(&addr), &len) == -1) {
        return false;
    }

    char ip[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == nullptr) {
        return false;
    }

    if (address) {
        *address = ip;
    }
    if (port) {
        *port = ntohs(addr.sin_port);
    }
    return true;
}

int LNativeTcpSocket::fd() const
{
    return m_fd;
}

bool LNativeTcpSocket::isOpen() const
{
    return m_fd != -1;
}

int LNativeTcpSocket::lastErrno() const
{
    return m_lastErrno;
}

std::string LNativeTcpSocket::lastErrorString() const
{
    return std::strerror(m_lastErrno);
}

void LNativeTcpSocket::recordErrno()
{
    m_lastErrno = errno;
}
