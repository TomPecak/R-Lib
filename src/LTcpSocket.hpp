#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "LEventLoop.hpp"
#include "LNativeTcpSocket.hpp"

/**
 * @brief TCP socket wrapper analogous to Qt's QTcpSocket.
 *
 * LTcpSocket handles the lifecycle of a connected TCP stream.
 * It maintains internal receive/transmit buffers so that I/O is
 * fully decoupled from epoll notifications.
 */
class LTcpSocket : public LEpollHandler
{
    friend class LTcpServer;

public:
    enum SocketState {
        UnconnectedState,
        HostLookupState,
        ConnectingState,
        ConnectedState,
        ClosingState
    };

    enum SocketError {
        UnknownSocketError,
        ConnectionRefusedError,
        RemoteHostClosedError,
        NetworkError,
        AddressInUseError,
        SocketAccessError,
        SocketResourceError
    };

    LTcpSocket();
    ~LTcpSocket() override;

    void connectToHost(const std::string &hostName, uint16_t port);
    void disconnectFromHost();
    void abort();

    /** Read up to @p maxSize bytes from the internal receive buffer. */
    int64_t read(char *data, int64_t maxSize);

    /** Read all remaining bytes from the internal receive buffer. */
    std::vector<uint8_t> readAll();

    /** Returns the number of bytes currently buffered for reading. */
    int64_t bytesAvailable() const;

    /**
     * @brief Enqueue data for asynchronous transmission.
     *
     * If the transmit buffer is empty, a non-blocking send() is attempted
     * immediately. Any unsent bytes are stored in the internal write buffer
     * and flushed automatically when EPOLLOUT fires.
     */
    int64_t write(const char *data, int64_t size);
    int64_t write(const std::vector<uint8_t> &data);

    SocketState state() const;
    SocketError error() const;
    std::string errorString() const;

    std::string localAddress() const;
    uint16_t localPort() const;

    std::string peerAddress() const;
    uint16_t peerPort() const;

    // Callbacks (Qt-style "signals")
    void onReadyRead(std::function<void()> callback);
    void onBytesWritten(std::function<void(int64_t bytes)> callback);
    void onConnected(std::function<void()> callback);
    void onDisconnected(std::function<void()> callback);
    void onErrorOccurred(std::function<void(SocketError)> callback);
    void onStateChanged(std::function<void(SocketState)> callback);

    template<typename Object>
    void onReadyRead(Object *obj, void (Object::*method)())
    {
        m_readyReadCallback = [obj, method]() { (obj->*method)(); };
    }

    template<typename Object>
    void onBytesWritten(Object *obj, void (Object::*method)(int64_t))
    {
        m_bytesWrittenCallback = [obj, method](int64_t bytes) { (obj->*method)(bytes); };
    }

    template<typename Object>
    void onConnected(Object *obj, void (Object::*method)())
    {
        m_connectedCallback = [obj, method]() { (obj->*method)(); };
    }

    template<typename Object>
    void onDisconnected(Object *obj, void (Object::*method)())
    {
        m_disconnectedCallback = [obj, method]() { (obj->*method)(); };
    }

    template<typename Object>
    void onErrorOccurred(Object *obj, void (Object::*method)(SocketError))
    {
        m_errorCallback = [obj, method](SocketError error) { (obj->*method)(error); };
    }

    template<typename Object>
    void onStateChanged(Object *obj, void (Object::*method)(SocketState))
    {
        m_stateCallback = [obj, method](SocketState state) { (obj->*method)(state); };
    }

protected:
    void handleEpollEvent(uint32_t events) override;

private:
    /** Adopt an already-accepted file descriptor (used by LTcpServer). */
    explicit LTcpSocket(int socket_fd);

    void setError(SocketError error, const std::string &errorString);
    void setErrorFromNative(SocketError error);
    void setState(SocketState state);
    void compactReadBufferIfNeeded();
    void compactWriteBufferIfNeeded();

    /** Re-register the socket with the desired epoll interest mask. */
    void updateEpollInterest(uint32_t events);

    /** Attempt to drain the internal write buffer into the kernel. */
    void flushWriteBuffer();

    bool m_registeredToEpoll = false;
    uint32_t m_epollInterest = 0;
    LNativeTcpSocket m_nativeSocket;

    SocketState m_state;
    SocketError m_error;
    std::string m_errorString;

    std::string m_localAddress;
    uint16_t m_localPort = 0;

    std::string m_peerAddress;
    uint16_t m_peerPort = 0;

    // Internal buffers decouple kernel I/O from user consumption.
    std::vector<uint8_t> m_readBuffer;
    size_t m_readStart = 0;
    // TODO: If STL vector-offset buffering is not sufficient, implement a custom fixed ring buffer.
    std::vector<uint8_t> m_writeBuffer;
    size_t m_writeStart = 0;

    // Callbacks
    std::function<void()> m_readyReadCallback;
    std::function<void(int64_t)> m_bytesWrittenCallback;
    std::function<void()> m_connectedCallback;
    std::function<void()> m_disconnectedCallback;
    std::function<void(SocketError)> m_errorCallback;
    std::function<void(SocketState)> m_stateCallback;
};
