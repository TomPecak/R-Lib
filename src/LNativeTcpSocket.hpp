#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

class LNativeTcpSocket
{
public:
    LNativeTcpSocket();
    explicit LNativeTcpSocket(int adoptedFd);
    ~LNativeTcpSocket();

    LNativeTcpSocket(const LNativeTcpSocket &) = delete;
    LNativeTcpSocket &operator=(const LNativeTcpSocket &) = delete;

    bool open();
    bool connectIPv4(const std::string &host, uint16_t port);

    ssize_t recvSome(void *buffer, size_t size, int flags = 0);
    ssize_t sendSome(const void *buffer, size_t size, int flags);

    bool shutdownReadWrite();
    void closeFd();

    bool loadLocalEndpoint(std::string *address, uint16_t *port) const;
    bool loadPeerEndpoint(std::string *address, uint16_t *port) const;

    int fd() const;
    bool isOpen() const;

    int lastErrno() const;
    std::string lastErrorString() const;

private:
    void recordErrno();

    int m_fd = -1;
    int m_lastErrno = 0;
};
