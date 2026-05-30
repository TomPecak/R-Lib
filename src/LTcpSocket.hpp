#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "LEventLoop.hpp"

class LTcpSocket : public LEpollHandler
{
public:
    enum SocketState {
        UnconnectedState,
        HostLookupState,
        ConnectingState,
        ConnectedState,
        BoundState,
        ListeningState,
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

    enum BindFlag { DefaultForPlatform = 0x0, ShareAddress = 0x1, ReuseAddressHint = 0x2 };

    LTcpSocket();
    ~LTcpSocket() override;

private:
    explicit LTcpSocket(int socket_fd);

public:
    // --- Server ---

    bool bind(uint16_t port, BindFlag mode = DefaultForPlatform);
    bool bind(const std::string &address, uint16_t port, BindFlag mode = DefaultForPlatform);

    bool listen(int backlog = 128);

    // Returns a new LTcpSocket for the accepted connection. Caller takes ownership.
    std::unique_ptr<LTcpSocket> accept();

    // --- Client ---

    void connectToHost(const std::string &hostName, uint16_t port);
    void disconnectFromHost();
    void abort();

    // --- I/O ---

    int64_t read(char *data, int64_t maxSize);
    std::vector<uint8_t> readAll();

    int64_t write(const char *data, int64_t size);
    int64_t write(const std::vector<uint8_t> &data);

    // --- State and Information ---

    SocketState state() const;
    SocketError error() const;
    std::string errorString() const;

    std::string localAddress() const;
    uint16_t localPort() const;

    std::string peerAddress() const;
    uint16_t peerPort() const;

    // --- Callbacks (Qt "Signals") ---

    void onReadyRead(std::function<void()> callback);
    void onBytesWritten(std::function<void(int64_t bytes)> callback);
    void onConnected(std::function<void()> callback);
    void onDisconnected(std::function<void()> callback);
    void onErrorOccurred(std::function<void(SocketError)> callback);
    void onStateChanged(std::function<void(SocketState)> callback);
    void onNewConnection(std::function<void()> callback);

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

    template<typename Object>
    void onNewConnection(Object *obj, void (Object::*method)())
    {
        m_newConnectionCallback = [obj, method]() { (obj->*method)(); };
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

    std::string m_localAddress;
    uint16_t m_localPort;

    std::string m_peerAddress;
    uint16_t m_peerPort;

    // Callbacks
    std::function<void()> m_readyReadCallback;
    std::function<void(int64_t)> m_bytesWrittenCallback;
    std::function<void()> m_connectedCallback;
    std::function<void()> m_disconnectedCallback;
    std::function<void(SocketError)> m_errorCallback;
    std::function<void(SocketState)> m_stateCallback;
    std::function<void()> m_newConnectionCallback;
};
