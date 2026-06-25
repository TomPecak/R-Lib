#include "LIcmpSocket.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

#include <chrono>
#include <thread>

TEST(LIcmpSocketTest, InitialStateIsUnconnected)
{
    LIcmpSocket socket;
    EXPECT_EQ(socket.state(), LIcmpSocket::UnconnectedState);
}

TEST(LIcmpSocketTest, RawSocketCreatedOrReportsAccessError)
{
    LIcmpSocket socket;

    if (getuid() != 0) {
        EXPECT_EQ(socket.error(), LIcmpSocket::SocketAccessError);
    } else {
        EXPECT_EQ(socket.error(), LIcmpSocket::UnknownSocketError);
        EXPECT_EQ(socket.state(), LIcmpSocket::UnconnectedState);
    }
}

TEST(LIcmpSocketTest, LocalEchoRequestAndReply)
{
    if (getuid() != 0) {
        GTEST_SKIP() << "Raw ICMP sockets require root privileges; skipping ping test.";
    }

    LIcmpSocket socket;
    ASSERT_EQ(socket.error(), LIcmpSocket::UnknownSocketError);

    const uint16_t id = 0xABCD;
    const uint16_t sequence = 1;
    const std::vector<uint8_t> payload = { 'R', 'L', 'i', 'b' };

    int64_t sent = socket.ping("127.0.0.1", id, sequence, payload);
    ASSERT_GT(sent, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LIcmpSocket::EchoReply reply;
    ASSERT_TRUE(socket.readEchoReply(&reply));

    EXPECT_EQ(reply.id, id);
    EXPECT_EQ(reply.sequence, sequence);
    EXPECT_EQ(reply.payload, payload);
    EXPECT_EQ(reply.address, "127.0.0.1");
}
