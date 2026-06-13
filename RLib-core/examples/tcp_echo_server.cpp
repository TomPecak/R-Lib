#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "LEventLoop.hpp"
#include "LTcpServer.hpp"
#include "LTcpSocket.hpp"

class ApplicationEchoTcp
{
public:
    ApplicationEchoTcp()
    {
        m_server.onNewConnection(this, &ApplicationEchoTcp::handleNewConnection);
    }

    bool start(uint16_t port)
    {
        if (m_server.listen("0.0.0.0", port)) {
            return true;
        }
        return false;
    }

private:
    void handleNewConnection()
    {
        while (std::unique_ptr<LTcpSocket> client = m_server.nextPendingConnection()) {
            LTcpSocket *clientPtr = client.get();

            clientPtr->onReadyRead([this, clientPtr]() { this->handleReadyRead(clientPtr); });

            clientPtr->onDisconnected([this, clientPtr]() { this->handleDisconnected(clientPtr); });

            m_clients.push_back(std::move(client));
        }
    }

    void handleReadyRead(LTcpSocket *client)
    {
        auto data = client->readAll();

        if (!data.empty()) {
            client->write(data);

            std::string text(data.begin(), data.end());
            std::cout << "[Klient " << client->peerPort() << "] napisał: " << text;
        }
    }

    void handleDisconnected(LTcpSocket *client)
    {
        auto it = std::remove_if(m_clients.begin(),
                                 m_clients.end(),
                                 [client](const std::unique_ptr<LTcpSocket> &ptr) {
                                     return ptr.get() == client;
                                 });

        if (it != m_clients.end()) {
            m_clients.erase(it, m_clients.end());
        }
    }

    LTcpServer m_server;
    std::vector<std::unique_ptr<LTcpSocket>> m_clients;
};

/*
 * =========================================================================
 * HOW TO TEST THIS EXAMPLE:
 * =========================================================================
 *
 * 1. Open a new terminal (Bash).
 * 2. Start Netcat (nc) on port 12345:
 * 
 *    nc 127.0.0.1 12345
 *
 * 3. Run this C++ application.
 * =========================================================================
*/

int main()
{
    LEventLoop loop;

    ApplicationEchoTcp app;
    if (!app.start(12345)) {
        return -1;
    }

    return loop.exec();
}