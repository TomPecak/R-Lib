#include <cerrno>
#include <cstring>
#include <iostream>

#include "LEventLoop.hpp"
#include "LIcmpSocket.hpp"
#include "LTimer.hpp"

class Pinger
{
public:
    Pinger()
        : m_sequence(0)
    {
        if (socket.error() != LIcmpSocket::UnknownSocketError) {
            std::cerr << "Failed to open ICMP socket: " << socket.errorString() << std::endl;
            std::cerr << "(raw ICMP sockets usually require root privileges or CAP_NET_RAW)"
                      << std::endl;
            return;
        }

        socket.onReadyRead(this, &Pinger::readPendingReplies);

        sendPing();
        timer.onTimeout(this, &Pinger::sendPing);
        timer.start(1000);
    }

    void sendPing()
    {
        ++m_sequence;
        std::string payload = "RLib ICMP ping test";

        int64_t bytes = socket.ping("127.0.0.1",
                                     0x4242,
                                     m_sequence,
                                     std::vector<uint8_t>(payload.begin(), payload.end()));

        if (bytes > 0) {
            std::cout << "Ping #" << m_sequence << " sent (" << bytes << " bytes)" << std::endl;
        } else {
            std::cerr << "Failed to send ping: " << socket.errorString() << std::endl;
        }
    }

    void readPendingReplies()
    {
        LIcmpSocket::EchoReply reply;
        while (socket.readEchoReply(&reply)) {
            std::cout << "Reply from " << reply.address
                      << ": id=" << reply.id
                      << " seq=" << reply.sequence;

            if (!reply.payload.empty()) {
                std::string text(reply.payload.begin(), reply.payload.end());
                std::cout << " payload=\"" << text << "\"";
            }
            std::cout << std::endl;
        }
    }

private:
    LIcmpSocket socket;
    LTimer timer;
    uint16_t m_sequence;
};

// Sends ICMP Echo Requests to 127.0.0.1 every second and prints replies.
// Run with root privileges or CAP_NET_RAW:
//
//     sudo ./core-05-icmp_ping
//
int main()
{
    LEventLoop loop;

    Pinger pinger;

    return loop.exec();
}
