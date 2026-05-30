#include <cerrno>
#include <cstring>
#include <iostream>

#include "../src/LEventLoop.hpp"
#include "../src/LTimer.hpp"
#include "../src/LUdpSocket.hpp"

using namespace std;

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

            // Remove trailing newline if it exists (e.g., from netcat)
            if (!text.empty() && text.back() == '\n') {
                text.pop_back();
            }

            std::cout << "Received: '" << text << "' from " << senderAddress << ":" << senderPort
                      << std::endl;

            // Send an echo reply back
            std::string reply = "ECHO: " + text + "\n";
            socket.writeDatagram(reply.c_str(), reply.length(), senderAddress, senderPort);
        }
    }

private:
    LUdpSocket socket;
};

/*
 * =========================================================================
 * HOW TO TEST THIS EXAMPLE:
 * =========================================================================
 * This application acts as a UDP Echo Server. It waits for incoming
 * packets, prints them, and sends them back to the sender.
 *
 * 1. Compile and RUN this C++ application first. 
 *    You should see: "UDP Server is listening on port 1234..."
 *
 * 2. Open a new terminal (Bash).
 * 3. Connect to the server using Netcat in UDP mode:
 * 
 *    nc -u 127.0.0.1 1234
 *
 * 4. Type any message in the terminal (e.g., "Hello R-Lib!") and press ENTER.
 * 
 * 5. Observe the results:
 *    - The C++ console will print "Received: 'Hello R-Lib!' from 127.0.0.1:xxxxx"
 *    - The Netcat terminal will instantly receive the reply: "ECHO: Hello R-Lib!"
 * =========================================================================
 */
int main()
{
    // The Event Loop must be the first thing instantiated!
    LEventLoop loop;

    // Start the server
    UdpServer udpServer;

    // Start processing asynchronous events
    return loop.exec();
}