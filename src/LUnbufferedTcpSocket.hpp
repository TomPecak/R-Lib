#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "LEventLoop.hpp"
#include "LNativeTcpSocket.hpp"

class LUnbufferedTcpSocket : public LEpollHandler
{
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

    LUnbufferedTcpSocket();
    explicit LUnbufferedTcpSocket(int socketFd);
    ~LUnbufferedTcpSocket() override;

    void connectToHost(const std::string &hostName, uint16_t port);
    void disconnectFromHost();
    void abort();

    int64_t read(char *data, int64_t maxSize);
    std::vector<uint8_t> readAll();
    int64_t bytesAvailable() const;

    int64_t write(const char *data, int64_t size);
    int64_t write(const std::vector<uint8_t> &data);

    SocketState state() const;
    SocketError error() const;
    std::string errorString() const;

    std::string localAddress() const;
    uint16_t localPort() const;

    std::string peerAddress() const;
    uint16_t peerPort() const;

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
    void setError(SocketError error, const std::string &errorString);
    void setErrorFromNative(SocketError error);
    void setState(SocketState state);
    void registerToEventLoop(uint32_t events);
    void unregisterFromEventLoop();
    void updateEndpoints();

    bool m_registeredToEpoll = false;
    LNativeTcpSocket m_nativeSocket;

    SocketState m_state = UnconnectedState;
    SocketError m_error = UnknownSocketError;
    std::string m_errorString;

    std::string m_localAddress;
    uint16_t m_localPort = 0;
    std::string m_peerAddress;
    uint16_t m_peerPort = 0;

    std::function<void()> m_readyReadCallback;
    std::function<void(int64_t)> m_bytesWrittenCallback;
    std::function<void()> m_connectedCallback;
    std::function<void()> m_disconnectedCallback;
    std::function<void(SocketError)> m_errorCallback;
    std::function<void(SocketState)> m_stateCallback;
};
