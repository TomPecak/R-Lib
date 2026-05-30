#include <cerrno>
#include <cstring>
#include <iostream>

#include "../src/LEventLoop.hpp"
#include "../src/LTimer.hpp"
#include "../src/LUdpSocket.hpp"

using namespace std;

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
        timer.start(1000); // 1000 ms = 1 second

        std::cout << "UdpSender started! Sending data to 127.0.0.1:5555 every 1s..." << std::endl;
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

/*
 * =========================================================================
 * HOW TO TEST THIS EXAMPLE:
 * =========================================================================
 * Because UDP is a connectionless protocol, the receiver must be running
 * and listening on the port BEFORE you start this application. Otherwise,
 * the sent packets will just be dropped by the operating system.
 *
 * 1. Open a new terminal (Bash).
 * 2. Start Netcat (nc) in UDP listen mode on port 5555:
 * 
 *    nc -u -l 5555
 *
 * 3. Run this C++ application.
 * 4. You should see "Message no. X from R-Lib!" appearing in the 
 *    Netcat terminal every second.
 * =========================================================================
 */
int main()
{
    // The Event Loop must be the first thing instantiated!
    LEventLoop loop;

    // Start our UDP spamer
    UdpSender sender;

    // Start processing asynchronous events
    return loop.exec();
}