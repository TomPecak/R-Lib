#include "LUdpSocket.hpp"
#include <gtest/gtest.h>

TEST(LUdpSocketTest, InitialStateIsUnconnected)
{
    LUdpSocket socket;
    EXPECT_EQ(socket.state(), LUdpSocket::UnconnectedState);
    EXPECT_EQ(socket.error(), LUdpSocket::UnknownSocketError);
}

TEST(LUdpSocketTest, StateChangesToConnected)
{
    LUdpSocket socket;
    socket.connectToHost("127.0.0.1", 5555);
    EXPECT_EQ(socket.state(), LUdpSocket::ConnectedState);

    socket.disconnectFromHost();
    EXPECT_EQ(socket.state(), LUdpSocket::UnconnectedState);
}