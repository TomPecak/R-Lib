#include "LUnbufferedTcpSocket.hpp"

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>

LUnbufferedTcpSocket::LUnbufferedTcpSocket()
{
    if (!m_nativeSocket.open()) {
        setErrorFromNative(SocketResourceError);
        return;
    }

    registerToEventLoop(EPOLLIN);
}

LUnbufferedTcpSocket::LUnbufferedTcpSocket(int socketFd)
    : m_nativeSocket(socketFd)
    , m_state(ConnectedState)
{
    updateEndpoints();
    registerToEventLoop(EPOLLIN);
}

LUnbufferedTcpSocket::~LUnbufferedTcpSocket()
{
    unregisterFromEventLoop();
    m_nativeSocket.closeFd();
}

void LUnbufferedTcpSocket::connectToHost(const std::string &hostName, uint16_t port)
{
    if (!m_nativeSocket.isOpen()) {
        return;
    }

    if (!m_nativeSocket.connectIPv4(hostName, port)) {
        if (m_nativeSocket.lastErrno() == EINPROGRESS) {
            m_peerAddress = hostName;
            m_peerPort = port;
            setState(ConnectingState);
            registerToEventLoop(EPOLLOUT);
            return;
        }

        setErrorFromNative(m_nativeSocket.lastErrno() == ECONNREFUSED ? ConnectionRefusedError
                                                                      : NetworkError);
        return;
    }

    m_peerAddress = hostName;
    m_peerPort = port;
    updateEndpoints();
    setState(ConnectedState);
    if (m_connectedCallback) {
        m_connectedCallback();
    }
}

void LUnbufferedTcpSocket::disconnectFromHost()
{
    if (!m_nativeSocket.isOpen()) {
        return;
    }

    unregisterFromEventLoop();
    m_nativeSocket.shutdownReadWrite();
    m_nativeSocket.closeFd();
    m_peerAddress.clear();
    m_peerPort = 0;

    setState(UnconnectedState);
    if (m_disconnectedCallback) {
        m_disconnectedCallback();
    }
}

void LUnbufferedTcpSocket::abort()
{
    disconnectFromHost();
}

int64_t LUnbufferedTcpSocket::read(char *data, int64_t maxSize)
{
    if (!m_nativeSocket.isOpen() || maxSize <= 0) {
        return 0;
    }

    ssize_t ret = m_nativeSocket.recvSome(data, static_cast<size_t>(maxSize), 0);
    if (ret > 0) {
        return ret;
    }

    if (ret == 0) {
        setError(RemoteHostClosedError, "Remote host closed the connection");
        setState(ClosingState);
        if (m_disconnectedCallback) {
            m_disconnectedCallback();
        }
        return 0;
    }

    if (m_nativeSocket.lastErrno() == EAGAIN || m_nativeSocket.lastErrno() == EWOULDBLOCK) {
        return 0;
    }

    setErrorFromNative(NetworkError);
    return -1;
}

std::vector<uint8_t> LUnbufferedTcpSocket::readAll()
{
    std::vector<uint8_t> buffer;
    uint8_t chunk[65536];

    while (true) {
        int64_t ret = read(reinterpret_cast<char *>(chunk), sizeof(chunk));
        if (ret > 0) {
            buffer.insert(buffer.end(), chunk, chunk + ret);
            continue;
        }
        break;
    }

    return buffer;
}

int64_t LUnbufferedTcpSocket::bytesAvailable() const
{
    if (!m_nativeSocket.isOpen()) {
        return 0;
    }

    int bytesAvailable = 0;
    if (::ioctl(m_nativeSocket.fd(), FIONREAD, &bytesAvailable) == -1) {
        return -1;
    }

    return bytesAvailable;
}

int64_t LUnbufferedTcpSocket::write(const char *data, int64_t size)
{
    if (!m_nativeSocket.isOpen() || m_state != ConnectedState) {
        return -1;
    }
    if (size <= 0) {
        return 0;
    }

    ssize_t ret = m_nativeSocket.sendSome(data, static_cast<size_t>(size), MSG_NOSIGNAL);
    if (ret > 0 && m_bytesWrittenCallback) {
        m_bytesWrittenCallback(ret);
    } else if (ret == -1 && m_nativeSocket.lastErrno() != EAGAIN
               && m_nativeSocket.lastErrno() != EWOULDBLOCK) {
        setErrorFromNative(m_nativeSocket.lastErrno() == EPIPE ? RemoteHostClosedError
                                                               : NetworkError);
    }

    return ret;
}

int64_t LUnbufferedTcpSocket::write(const std::vector<uint8_t> &data)
{
    return write(reinterpret_cast<const char *>(data.data()), static_cast<int64_t>(data.size()));
}

LUnbufferedTcpSocket::SocketState LUnbufferedTcpSocket::state() const
{
    return m_state;
}

LUnbufferedTcpSocket::SocketError LUnbufferedTcpSocket::error() const
{
    return m_error;
}

std::string LUnbufferedTcpSocket::errorString() const
{
    return m_errorString;
}

std::string LUnbufferedTcpSocket::localAddress() const
{
    return m_localAddress;
}

uint16_t LUnbufferedTcpSocket::localPort() const
{
    return m_localPort;
}

std::string LUnbufferedTcpSocket::peerAddress() const
{
    return m_peerAddress;
}

uint16_t LUnbufferedTcpSocket::peerPort() const
{
    return m_peerPort;
}

void LUnbufferedTcpSocket::onReadyRead(std::function<void()> callback)
{
    m_readyReadCallback = callback;
}

void LUnbufferedTcpSocket::onBytesWritten(std::function<void(int64_t)> callback)
{
    m_bytesWrittenCallback = callback;
}

void LUnbufferedTcpSocket::onConnected(std::function<void()> callback)
{
    m_connectedCallback = callback;
}

void LUnbufferedTcpSocket::onDisconnected(std::function<void()> callback)
{
    m_disconnectedCallback = callback;
}

void LUnbufferedTcpSocket::onErrorOccurred(std::function<void(SocketError)> callback)
{
    m_errorCallback = callback;
}

void LUnbufferedTcpSocket::onStateChanged(std::function<void(SocketState)> callback)
{
    m_stateCallback = callback;
}

void LUnbufferedTcpSocket::handleEpollEvent(uint32_t events)
{
    if (events & EPOLLERR) {
        int errorCode = 0;
        socklen_t len = sizeof(errorCode);
        if (::getsockopt(m_nativeSocket.fd(), SOL_SOCKET, SO_ERROR, &errorCode, &len) == 0
            && errorCode != 0) {
            setError(errorCode == ECONNREFUSED ? ConnectionRefusedError : NetworkError,
                     std::strerror(errorCode));
        }
        setState(UnconnectedState);
        disconnectFromHost();
        return;
    }

    if (events & EPOLLOUT) {
        if (m_state == ConnectingState) {
            updateEndpoints();
            setState(ConnectedState);
            registerToEventLoop(EPOLLIN);
            if (m_connectedCallback) {
                m_connectedCallback();
            }
        }
    }

    if ((events & EPOLLIN) && m_readyReadCallback) {
        m_readyReadCallback();
    }

    if (events & EPOLLHUP) {
        setError(RemoteHostClosedError, "Connection hung up");
        setState(ClosingState);
        if (m_disconnectedCallback) {
            m_disconnectedCallback();
        }
    }
}

void LUnbufferedTcpSocket::setError(SocketError error, const std::string &errorString)
{
    m_error = error;
    m_errorString = errorString;
    if (m_errorCallback) {
        m_errorCallback(m_error);
    }
}

void LUnbufferedTcpSocket::setErrorFromNative(SocketError error)
{
    setError(error, m_nativeSocket.lastErrorString());
}

void LUnbufferedTcpSocket::setState(SocketState state)
{
    if (m_state != state) {
        m_state = state;
        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

void LUnbufferedTcpSocket::registerToEventLoop(uint32_t events)
{
    if (!m_nativeSocket.isOpen()) {
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (!loop) {
        return;
    }

    if (m_registeredToEpoll) {
        m_registeredToEpoll = loop->modifyHandler(m_nativeSocket.fd(), events, this);
    } else {
        m_registeredToEpoll = loop->registerHandler(m_nativeSocket.fd(), events, this);
    }
}

void LUnbufferedTcpSocket::unregisterFromEventLoop()
{
    if (!m_nativeSocket.isOpen() || !m_registeredToEpoll) {
        return;
    }

    LEventLoop *loop = LEventLoop::current();
    if (loop) {
        loop->unregisterHandler(m_nativeSocket.fd());
    }
    m_registeredToEpoll = false;
}

void LUnbufferedTcpSocket::updateEndpoints()
{
    m_nativeSocket.loadLocalEndpoint(&m_localAddress, &m_localPort);
    m_nativeSocket.loadPeerEndpoint(&m_peerAddress, &m_peerPort);
}
