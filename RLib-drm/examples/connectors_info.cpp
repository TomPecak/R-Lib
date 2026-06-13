#include <iostream>

#include <LDrmDevice.hpp>

int main(int argc, char *argv[])
{
    LDrmDevice gpu;
    gpu.openAuto();

    std::cout << std::endl << "FOUND CARD: " << gpu.deviceName() << std::endl << std::endl;

    std::cout << "ALL CONNECTORS:" << std::endl;
    auto allConnectors = gpu.allConnectors();
    for (const auto &connector : allConnectors) {
        std::cout << "Connector: " << connector.name << " " << connector.displayWidth << "x"
                  << connector.displayHeight << "@" << connector.displayRefreshRate << "Hz"
                  << std::endl;
    }

    std::cout << std::endl << "CONNECTED CONNECTORS:" << std::endl;
    auto connectedConnectors = gpu.connectedConnectors();
    for (const auto &connector : connectedConnectors) {
        std::cout << "Connected connector: " << connector.name << " " << connector.displayWidth
                  << "x" << connector.displayHeight << "@" << connector.displayRefreshRate << "Hz"
                  << std::endl;
    }

    std::cout << std::endl << "PRIMARY CONNECTOR:" << std::endl;
    LConnectorInfo primaryConnector = gpu.primaryConnector();
    std::cout << "Primary connector: " << primaryConnector.name << " "
              << primaryConnector.displayWidth << "x" << primaryConnector.displayHeight << "@"
              << primaryConnector.displayRefreshRate << "Hz" << std::endl;

    std::cout << std::endl;

    return 0;
}
