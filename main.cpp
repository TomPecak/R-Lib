//Linux
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

//C++
#include <cerrno>
#include <cstring>
#include <iostream>

#include "./src/LEventLoop.hpp"
#include "./src/LGpioPin.hpp"
#include "./src/LTimer.hpp"
#include "./src/LUdpSocket.hpp"

using namespace std;

class Application
{
public:
    Application()
        : tick_count_(0)
    {
        //timer.onTimeout([this]() { this->handle_timeout(); });
        timer.onTimeout(this, &Application::handle_timeout);
        timer.start(1);
    }

    void handle_timeout()
    {
        tick_count_++;

        // Every 1000 calls (approximately every 1 second), print information to the screen
        if (tick_count_ % 1000 == 0) {
            std::cout << "Application::LTimer Call no: " << tick_count_ << " (Elapsed approx. "
                      << tick_count_ / 1000 << " s)" << std::endl;
        }
    }

private:
    LTimer timer;
    uint64_t tick_count_;
};

class UdpServer
{
public:
    UdpServer()
    {
        // Listen on port 1234
        if (socket.bind(1234)) {
            std::cout << "UDP Server is listening on port 1234..." << std::endl;
        }

        // Using template magic - beautifully "connecting" the slot!
        socket.onReadyRead(this, &UdpServer::readPendingDatagrams);
    }

    void readPendingDatagrams()
    {
        while (socket.hasPendingDatagrams()) {
            std::string senderAddress;
            uint16_t senderPort;

            // Receive a vector of bytes
            auto datagram = socket.receiveDatagram(&senderAddress, &senderPort);

            std::string text(datagram.begin(), datagram.end());
            std::cout << "Received: '" << text << "' from " << senderAddress << ":" << senderPort
                      << std::endl;

            // Send an echo reply back
            std::string reply = "ECHO: " + text;
            socket.writeDatagram(reply.c_str(), reply.length(), senderAddress, senderPort);
        }
    }

private:
    LUdpSocket socket;
};

class UdpSender
{
public:
    UdpSender()
        : counter(0)
    {
        // Hardcode the socket configuration to send to a specific address and port.
        // Thanks to connectToHost(), the Linux kernel will remember this address.
        socket.connectToHost("127.0.0.1", 5555);

        // Set the timer to tick every 1 second (1000 ms)
        timer.onTimeout(this, &UdpSender::sendData);
        timer.start(10); // Uwaga: w kodzie masz 10 ms, ale w tekście wyżej 1s ;)

        std::cout << "UdpSender started! Sending data to port 5555 every 1s..." << std::endl;
    }

    void sendData()
    {
        counter++;
        std::string message = "Message no. " + std::to_string(counter) + " from R-Lib!\n";

        // Since we used connectToHost, we can just call the fast write(),
        // instead of writeDatagram() which requires providing the IP and port every time.
        int64_t bytes = socket.write(message.c_str(), message.length());

        if (bytes > 0) {
            std::cout << "Sent: " << message;
        } else {
            std::cerr << "Send error: " << socket.errorString() << std::endl;
        }
    }

private:
    LUdpSocket socket;
    LTimer timer;
    int counter;
};

int main()
{
    LEventLoop loop;

    // LGpioPin ledPin("/dev/gpiochip4", 17);
    // ledPin.setDirection(LGpioPin::Output);

    //auto app_3 = new Application();

    //------------------------------

    // LTimer timer_1;
    // timer_1.onTimeout([]() {
    //     std::cout << "Quit application!" << std::endl;
    //     LEventLoop::quit();
    // });
    // timer_1.start(5 * 1000);

    // LTimer timer_2;
    // timer_2.onTimeout([]() { std::cout << "Timer_2 event!" << std::endl; });
    // timer_2.start(150);

    // Application app;
    // Application app_2;

    UdpSender UdpSender;

    return loop.exec();
}