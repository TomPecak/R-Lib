#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "LEventLoop.hpp"

// ICMP raw socket for IPv4 echo request/reply.
class LIcmpSocket : public LEpollHandler
{
public:
    enum SocketState {
        UnconnectedState,
        HostLookupState,
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

    struct EchoReply {
        uint16_t id = 0;
        uint16_t sequence = 0;
        std::vector<uint8_t> payload;
        std::string address;
    };

    LIcmpSocket();
    ~LIcmpSocket() override;

    bool bind(const std::string &address);

    void connectToHost(const std::string &hostName);
    void disconnectFromHost();
    void abort();

    bool hasPendingDatagrams() const;
    int64_t pendingDatagramSize() const;

    int64_t readDatagram(char *data,
                         int64_t maxSize,
                         std::string *address = nullptr);
    std::vector<uint8_t> receiveDatagram(std::string *address = nullptr);

    int64_t ping(const std::string &address,
                 uint16_t id,
                 uint16_t sequence,
                 const std::vector<uint8_t> &payload = {});
    bool readEchoReply(EchoReply *reply);

    int64_t write(const char *data, int64_t size);
    int64_t writeDatagram(const char *data,
                          int64_t size,
                          const std::string &address);

    SocketState state() const;
    SocketError error() const;
    std::string errorString() const;

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
    void handleEpollEvent(uint32_t events) override;

private:
    void setError(SocketError error, const std::string &errorString);
    void setState(SocketState state);

    bool parseEchoReply(const std::vector<uint8_t> &datagram, EchoReply *reply) const;

    int m_socket_fd;
    SocketState m_state;
    SocketError m_error;
    std::string m_errorString;

    std::string m_peerAddress;

    std::function<void()> m_readyReadCallback;
    std::function<void(int64_t)> m_bytesWrittenCallback;
    std::function<void(SocketError)> m_errorCallback;
    std::function<void(SocketState)> m_stateCallback;
};
