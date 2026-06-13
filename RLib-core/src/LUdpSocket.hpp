#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "LEventLoop.hpp"

class LUdpSocket : public LEpollHandler
{
public:
    enum SocketState {
        UnconnectedState,
        HostLookupState, // Optional for future asynchronous DNS support
        ConnectingState,
        ConnectedState,
        BoundState
    };

    enum SocketError {
        UnknownSocketError,
        ConnectionRefusedError,
        DatagramTooLargeError,
        NetworkError,
        AddressInUseError,
        SocketAccessError,
        SocketResourceError
    };

    enum BindFlag { DefaultForPlatform = 0x0, ShareAddress = 0x1, ReuseAddressHint = 0x2 };

    LUdpSocket();
    ~LUdpSocket() override;

    // --- Connection and Binding Management ---

    // Binds the socket to a specific port (listens on all interfaces "0.0.0.0")
    bool bind(uint16_t port, BindFlag mode = DefaultForPlatform);

    // Binds the socket to a specific IP address and port
    bool bind(const std::string &address, uint16_t port, BindFlag mode = DefaultForPlatform);

    // UDP is a connectionless protocol, but a kernel-level "connect"
    // sets a default destination address (allows using a simple write() instead of sendto())
    void connectToHost(const std::string &hostName, uint16_t port);
    void disconnectFromHost();
    void abort();

    // --- Reading (QUdpSocket-style Interface) ---

    bool hasPendingDatagrams() const;
    int64_t pendingDatagramSize() const;

    // Reads into a raw buffer
    int64_t readDatagram(char *data,
                         int64_t maxSize,
                         std::string *address = nullptr,
                         uint16_t *port = nullptr);

    // Convenient "Qt-style" read, omitting QByteArray
    std::vector<uint8_t> receiveDatagram(std::string *address = nullptr, uint16_t *port = nullptr);

    // --- Writing (QUdpSocket-style Interface) ---

    // Writes from a raw pointer
    int64_t writeDatagram(const char *data, int64_t size, const std::string &address, uint16_t port);

    // Writes a vector (equivalent to QByteArray)
    int64_t writeDatagram(const std::vector<uint8_t> &datagram,
                          const std::string &address,
                          uint16_t port);

    // Writes for "connected" sockets (when connectToHost was used)
    int64_t write(const char *data, int64_t size);

    // --- State and Information ---

    SocketState state() const;
    SocketError error() const;
    std::string errorString() const;

    // --- Callbacks (Qt "Signals") ---

    void onReadyRead(std::function<void()> callback);
    void onBytesWritten(std::function<void(int64_t bytes)> callback);
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
    // Handles epoll events
    void handleEpollEvent(uint32_t events) override;

private:
    void setError(SocketError error, const std::string &errorString);
    void setState(SocketState state);

    int m_socket_fd;
    SocketState m_state;
    SocketError m_error;
    std::string m_errorString;

    // Variables for "connected" UDP
    std::string m_peerAddress;
    uint16_t m_peerPort;

    // Callbacks
    std::function<void()> m_readyReadCallback;
    std::function<void(int64_t)> m_bytesWrittenCallback;
    std::function<void(SocketError)> m_errorCallback;
    std::function<void(SocketState)> m_stateCallback;
};