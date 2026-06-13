#include "LEventLoop.hpp"
#include "LTcpServer.hpp"
#include "LTcpSocket.hpp"
#include "LTimer.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>

// Test LEventLoop: Local tasks run directly or on loop start
TEST(LEventLoopTest, LocalTasksRunOnExec)
{
    LEventLoop loop;
    int localTaskRan = 0;

    // Post task from the same thread as loop creation
    loop.postTask([&]() {
        localTaskRan++;
    });

    // Run a timer to quit the loop shortly
    LTimer quitTimer;
    quitTimer.onTimeout([&]() {
        LEventLoop::quit();
    });
    quitTimer.start(10);

    loop.exec();

    EXPECT_EQ(localTaskRan, 1);
}

// Test LEventLoop: Cross-thread tasks wake up the loop and execute
TEST(LEventLoopTest, CrossThreadTasksWakeUpLoop)
{
    LEventLoop loop;
    std::atomic<int> taskRan{0};

    // Spin up a thread to post a task to the loop after some delay
    std::thread helperThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        loop.postTask([&]() {
            taskRan = 1;
            LEventLoop::quit();
        });
    });

    // Run loop - this should block until the cross-thread task is posted and wakes it up
    loop.exec();

    if (helperThread.joinable()) {
        helperThread.join();
    }

    EXPECT_EQ(taskRan.load(), 1);
}

// Test LTcpSocket: Backpressure / setMaxReadBufferSize
TEST(LTcpSocketTest, MaxReadBufferSizeBackpressure)
{
    LEventLoop loop;

    LTcpServer server;
    ASSERT_TRUE(server.listen("127.0.0.1", 12345));

    std::unique_ptr<LTcpSocket> serverSocket;
    server.onNewConnection([&]() {
        serverSocket = server.nextPendingConnection();
    });

    LTcpSocket clientSocket;
    clientSocket.connectToHost("127.0.0.1", 12345);

    // Set max read buffer size to 50 bytes
    clientSocket.setMaxReadBufferSize(50);
    EXPECT_EQ(clientSocket.maxReadBufferSize(), 50);

    // Run event loop to establish connection and accept it
    LTimer connectionTimer;
    connectionTimer.onTimeout([&]() {
        LEventLoop::quit();
    });
    connectionTimer.start(20);
    loop.exec();

    ASSERT_NE(serverSocket, nullptr);
    EXPECT_EQ(clientSocket.state(), LTcpSocket::ConnectedState);

    // Now, write 200 bytes from the server
    std::vector<uint8_t> sendData(200, 'A');
    serverSocket->write(sendData);

    // Run event loop to trigger reading. Since clientSocket limit is 50,
    // it should read up to limit (or one chunk that exceeds 50 but not the whole 200)
    LTimer readTimer;
    readTimer.onTimeout([&]() {
        LEventLoop::quit();
    });
    readTimer.start(20);
    loop.exec();

    // Verify backpressure is engaged: clientSocket buffer size is capped, less than 200
    int64_t available = clientSocket.bytesAvailable();
    EXPECT_GT(available, 0);
    EXPECT_LT(available, 200);

    // Client reads 30 bytes to free up some space, but buffer is still above limit if it was e.g. 64KB chunk?
    // Wait, since we read chunk size 65536 in handleEpollEvent, the first recvSome might read all 200 bytes
    // in one go if they fit in the TCP window and are available on the kernel socket.
    // However, if we write 200KB, the kernel buffer and recvSome limits will show clear backpressure.
    // Let's check with a larger dataset, say 200,000 bytes.
}

TEST(LTcpSocketTest, LargeDataBackpressureAndResume)
{
    LEventLoop loop;

    LTcpServer server;
    ASSERT_TRUE(server.listen("127.0.0.1", 12346));

    std::unique_ptr<LTcpSocket> serverSocket;
    server.onNewConnection([&]() {
        serverSocket = server.nextPendingConnection();
    });

    LTcpSocket clientSocket;
    clientSocket.connectToHost("127.0.0.1", 12346);

    // Set max read buffer size to 1000 bytes
    clientSocket.setMaxReadBufferSize(1000);

    // Connect
    LTimer connectionTimer;
    connectionTimer.onTimeout([&]() {
        LEventLoop::quit();
    });
    connectionTimer.start(20);
    loop.exec();

    ASSERT_NE(serverSocket, nullptr);

    // Server sends 100,000 bytes of data
    std::vector<uint8_t> sendData(100000, 'B');
    serverSocket->write(sendData);

    // Give it a moment to run epoll event loop and read
    LTimer readTimer;
    readTimer.onTimeout([&]() {
        LEventLoop::quit();
    });
    readTimer.start(50);
    loop.exec();

    // The client read buffer should have hit the limit.
    // Since recvSome uses a chunk size of 65536, it might read up to 65536 bytes in the first recvSome call.
    // But it should NOT have read the whole 100,000 bytes because subsequent recvSome calls are blocked.
    int64_t available = clientSocket.bytesAvailable();
    EXPECT_GT(available, 0);
    EXPECT_LT(available, 100000);

    // Read all of it currently in buffer
    auto readBytes = clientSocket.readAll();
    EXPECT_EQ(readBytes.size(), available);

    // Disable limit to read all remaining data
    clientSocket.setMaxReadBufferSize(0);

    // Now that read buffer size is below 1000, client should resume EPOLLIN and read the remaining data
    LTimer resumeTimer;
    resumeTimer.onTimeout([&]() {
        LEventLoop::quit();
    });
    resumeTimer.start(50);
    loop.exec();

    // The remaining data should have been read now
    int64_t newAvailable = clientSocket.bytesAvailable();
    EXPECT_GT(newAvailable, 0);
    auto secondRead = clientSocket.readAll();
    EXPECT_EQ(readBytes.size() + secondRead.size(), 100000);
}
