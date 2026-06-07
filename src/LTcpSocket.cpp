#include "LTcpSocket.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

LTcpSocket::LTcpSocket()
    : m_state(UnconnectedState)
    , m_error(UnknownSocketError)
    , m_epollInterest(EPOLLIN)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    if (!m_nativeSocket.open()) {
        setErrorFromNative(SocketResourceError);
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        m_registeredToEpoll = loop->registerHandler(m_nativeSocket.fd(), EPOLLIN, this);
    } else {
        std::cerr << "LTcpSocket Warning: No LEventLoop in current thread!" << std::endl;
    }
}

LTcpSocket::LTcpSocket(int socket_fd)
    : m_state(ConnectedState)
    , m_error(UnknownSocketError)
    , m_epollInterest(EPOLLIN)
    , m_nativeSocket(socket_fd)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    m_nativeSocket.loadPeerEndpoint(&m_peerAddress, &m_peerPort);
    m_nativeSocket.loadLocalEndpoint(&m_localAddress, &m_localPort);

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        m_registeredToEpoll = loop->registerHandler(m_nativeSocket.fd(), EPOLLIN, this);
    } else {
        std::cerr << "LTcpSocket Warning: No LEventLoop in current thread!" << std::endl;
    }
}

LTcpSocket::~LTcpSocket()
{
    if (m_nativeSocket.isOpen()) {
        LEventLoop *loop = LEventLoop::current();
        if (loop && m_registeredToEpoll) {
            loop->unregisterHandler(m_nativeSocket.fd());
            m_registeredToEpoll = false;
        }
    }
    m_nativeSocket.closeFd();
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

void LTcpSocket::updateEpollInterest(uint32_t events)
{
    if (!m_nativeSocket.isOpen()) {
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (!loop) {
        return;
    }

    if (!m_registeredToEpoll) {
        m_registeredToEpoll = loop->registerHandler(m_nativeSocket.fd(), events, this);
    } else {
        m_registeredToEpoll = loop->modifyHandler(m_nativeSocket.fd(), events, this);
    }
    if (m_registeredToEpoll) {
        m_epollInterest = events;
    }
}

void LTcpSocket::connectToHost(const std::string &hostName, uint16_t port)
{
    if (!m_nativeSocket.isOpen()) {
        return;
    }

    if (!m_nativeSocket.connectIPv4(hostName, port)) {
        if (m_nativeSocket.lastErrno() == EINPROGRESS) {
            m_peerAddress = hostName;
            m_peerPort = port;
            setState(ConnectingState);
            updateEpollInterest(EPOLLOUT);
            return;
        }
        if (m_nativeSocket.lastErrno() == ECONNREFUSED) {
            setErrorFromNative(ConnectionRefusedError);
        } else {
            setErrorFromNative(NetworkError);
        }
        return;
    }

    m_peerAddress = hostName;
    m_peerPort = port;
    m_nativeSocket.loadLocalEndpoint(&m_localAddress, &m_localPort);
    setState(ConnectedState);
    if (m_connectedCallback) {
        m_connectedCallback();
    }
}

void LTcpSocket::disconnectFromHost()
{
    if (!m_nativeSocket.isOpen()) {
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop && m_registeredToEpoll) {
        loop->unregisterHandler(m_nativeSocket.fd());
        m_registeredToEpoll = false;
    }

    m_nativeSocket.shutdownReadWrite();
    m_nativeSocket.closeFd();

    m_readBuffer.clear();
    m_writeBuffer.clear();
    m_writeStart = 0;
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
    size_t available = m_readBuffer.size();
    if (maxSize <= 0 || available == 0) {
        return 0;
    }

    size_t bytesToRead = std::min<size_t>(static_cast<size_t>(maxSize), available);
    m_readBuffer.read(reinterpret_cast<uint8_t *>(data), bytesToRead);

    if (m_maxReadBufferSize > 0 && m_readBuffer.size() < m_maxReadBufferSize) {
        if (!(m_epollInterest & EPOLLIN)) {
            m_epollInterest |= EPOLLIN;
            updateEpollInterest(m_epollInterest);
        }
    }
    return static_cast<int64_t>(bytesToRead);
}

std::vector<uint8_t> LTcpSocket::readAll()
{
    auto data = m_readBuffer.readAll();
    if (m_maxReadBufferSize > 0 && m_readBuffer.size() < m_maxReadBufferSize) {
        if (!(m_epollInterest & EPOLLIN)) {
            m_epollInterest |= EPOLLIN;
            updateEpollInterest(m_epollInterest);
        }
    }
    return data;
}

int64_t LTcpSocket::bytesAvailable() const
{
    return static_cast<int64_t>(m_readBuffer.size());
}

void LTcpSocket::setMaxReadBufferSize(size_t limit)
{
    m_maxReadBufferSize = limit;
    if (m_maxReadBufferSize == 0 || m_readBuffer.size() < m_maxReadBufferSize) {
        if (!(m_epollInterest & EPOLLIN)) {
            m_epollInterest |= EPOLLIN;
            updateEpollInterest(m_epollInterest);
        }
    }
}

size_t LTcpSocket::maxReadBufferSize() const
{
    return m_maxReadBufferSize;
}

int64_t LTcpSocket::write(const char *data, int64_t size)
{
    if (!m_nativeSocket.isOpen() || m_state != ConnectedState) {
        return -1;
    }
    if (size <= 0) {
        return 0;
    }

    if ((m_writeBuffer.size() - m_writeStart) == 0) {
        ssize_t ret = m_nativeSocket.sendSome(data, static_cast<size_t>(size), MSG_NOSIGNAL);
        if (ret > 0) {
            if (m_bytesWrittenCallback) {
                m_bytesWrittenCallback(ret);
            }
            if (ret < size) {
                m_writeBuffer.insert(m_writeBuffer.end(), data + ret, data + size);
                updateEpollInterest(EPOLLIN | EPOLLOUT);
            }
            return size;
        }
        if (ret == -1) {
            if (m_nativeSocket.lastErrno() != EAGAIN && m_nativeSocket.lastErrno() != EWOULDBLOCK) {
                if (m_nativeSocket.lastErrno() == EPIPE || m_nativeSocket.lastErrno() == ECONNRESET) {
                    setErrorFromNative(RemoteHostClosedError);
                    setState(ClosingState);
                    if (m_disconnectedCallback) {
                        m_disconnectedCallback();
                    }
                } else {
                    setErrorFromNative(NetworkError);
                }
                return -1;
            }
        }
    }

    m_writeBuffer.insert(m_writeBuffer.end(), data, data + size);
    updateEpollInterest(EPOLLIN | EPOLLOUT);
    return size;
}

int64_t LTcpSocket::write(const std::vector<uint8_t> &data)
{
    return write(reinterpret_cast<const char *>(data.data()), static_cast<int64_t>(data.size()));
}

void LTcpSocket::flushWriteBuffer()
{
    if (!m_nativeSocket.isOpen() || (m_writeBuffer.size() - m_writeStart) == 0) {
        return;
    }

    while ((m_writeBuffer.size() - m_writeStart) > 0) {
        size_t remaining = m_writeBuffer.size() - m_writeStart;
        ssize_t ret = m_nativeSocket.sendSome(m_writeBuffer.data() + m_writeStart, remaining, MSG_NOSIGNAL);
        if (ret > 0) {
            if (m_bytesWrittenCallback) {
                m_bytesWrittenCallback(ret);
            }
            m_writeStart += static_cast<size_t>(ret);
            compactWriteBufferIfNeeded();
            continue;
        }

        if (ret == -1 && (m_nativeSocket.lastErrno() == EAGAIN || m_nativeSocket.lastErrno() == EWOULDBLOCK)) {
            break;
        }

        if (m_nativeSocket.lastErrno() == EPIPE || m_nativeSocket.lastErrno() == ECONNRESET) {
            setErrorFromNative(RemoteHostClosedError);
            setState(ClosingState);
            if (m_disconnectedCallback) {
                m_disconnectedCallback();
            }
            return;
        }

        setErrorFromNative(NetworkError);
        return;
    }

    if ((m_writeBuffer.size() - m_writeStart) == 0) {
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
        int errorCode = 0;
        socklen_t len = sizeof(errorCode);
        if (::getsockopt(m_nativeSocket.fd(), SOL_SOCKET, SO_ERROR, &errorCode, &len) == 0 && errorCode != 0) {
            if (errorCode == ECONNREFUSED) {
                setError(ConnectionRefusedError, std::strerror(errorCode));
            } else {
                setError(NetworkError, std::strerror(errorCode));
            }
        }

        if (m_state == ConnectingState) {
            setState(UnconnectedState);
        }

        if (m_nativeSocket.isOpen()) {
            LEventLoop *loop = LEventLoop::current();
            if (loop && m_registeredToEpoll) {
                loop->unregisterHandler(m_nativeSocket.fd());
                m_registeredToEpoll = false;
            }
            m_nativeSocket.closeFd();
        }

        if (m_disconnectedCallback) {
            m_disconnectedCallback();
        }
        return;
    }

    if (events & EPOLLOUT) {
        if (m_state == ConnectingState) {
            setState(ConnectedState);
            m_nativeSocket.loadLocalEndpoint(&m_localAddress, &m_localPort);
            if (m_connectedCallback) {
                m_connectedCallback();
            }
            if ((m_writeBuffer.size() - m_writeStart) == 0) {
                updateEpollInterest(EPOLLIN);
            } else {
                updateEpollInterest(EPOLLIN | EPOLLOUT);
            }
        } else if (m_state == ConnectedState) {
            flushWriteBuffer();
        }
    }

    if (events & EPOLLIN) {
        if (m_state == ConnectedState || m_state == ClosingState) {
            uint8_t chunk[65536];
            while (true) {
                size_t toRead = sizeof(chunk);
                if (m_maxReadBufferSize > 0) {
                    if (m_readBuffer.size() >= m_maxReadBufferSize) {
                        m_epollInterest &= ~EPOLLIN;
                        updateEpollInterest(m_epollInterest);
                        break;
                    }
                    toRead = std::min<size_t>(toRead, m_maxReadBufferSize - m_readBuffer.size());
                }

                ssize_t ret = m_nativeSocket.recvSome(chunk, toRead, 0);
                if (ret > 0) {
                    m_readBuffer.append(chunk, static_cast<size_t>(ret));
                    continue;
                }

                if (ret == 0) {
                    setError(RemoteHostClosedError, "Remote host closed the connection");
                    setState(ClosingState);
                    if (m_disconnectedCallback) {
                        m_disconnectedCallback();
                    }
                    break;
                }

                if (m_nativeSocket.lastErrno() == EAGAIN || m_nativeSocket.lastErrno() == EWOULDBLOCK) {
                    break;
                }
                setErrorFromNative(NetworkError);
                break;
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

void LTcpSocket::setErrorFromNative(SocketError error)
{
    setError(error, m_nativeSocket.lastErrorString());
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

void LTcpSocket::compactWriteBufferIfNeeded()
{
    if (m_writeStart == 0) {
        return;
    }
    if (m_writeStart < 65536 && m_writeStart * 2 < m_writeBuffer.size()) {
        return;
    }

    m_writeBuffer.erase(m_writeBuffer.begin(), m_writeBuffer.begin() + static_cast<std::ptrdiff_t>(m_writeStart));
    m_writeStart = 0;
}
