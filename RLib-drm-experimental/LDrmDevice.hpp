#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct gbm_device;

struct LScreenInfo
{
    uint32_t connectorId = 0; // Internal ID in the DRM system
    std::string name;         // e.g., "HDMI-A-1", "eDP-1"

    bool connected = false;

    uint32_t width = 0; // Highest (native) resolution
    uint32_t height = 0;
    uint32_t refreshRate = 0; // e.g., 60 (Hz)

    uint32_t physicalWidthMm = 0; // Physical dimensions (useful for UI/DPI scaling)
    uint32_t physicalHeightMm = 0;
};

class LDrmDevice
{
public:
    LDrmDevice();
    ~LDrmDevice();

    void openAuto();
    // Opens a specific file, e.g., "/dev/dri/card0"
    bool open(const std::string &nodePath);
    void close();

    bool isOpen() const;
    int fd() const; // DRM file descriptor (very important!)

    LScreenInfo primaryScreen() const;

    std::vector<LScreenInfo> allScreens() const;
    std::vector<LScreenInfo> connectedScreens() const;

    // getConnectedOutputs()

    gbm_device *gbmDevice() const;

    std::string deviceName() const;

private:
    int m_fd = -1;
    gbm_device *m_gbm = nullptr;
};