#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <LEventLoop.hpp>

class LScreenSurface;
struct gbm_device;

struct LDisplayMode
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refreshRate = 0;
};

struct LConnectorInfo
{
    uint32_t connectorId = 0; // Internal ID in the DRM system
    std::string name;         // e.g., "HDMI-A-1", "eDP-1"

    bool connected = false;

    uint32_t displayWidth = 0; // Highest (native) resolution
    uint32_t displayHeight = 0;
    uint32_t displayRefreshRate = 0; // e.g., 60 (Hz)

    uint32_t displayPhysicalWidthMm = 0; // Physical dimensions (useful for UI/DPI scaling)
    uint32_t displayPhysicalHeightMm = 0;

    std::vector<LDisplayMode> availableModes;
};

class LDrmDevice : public LEpollHandler
{
    //access to register and unregister surface methodts
    friend class LScreenSurface;

public:
    LDrmDevice();
    ~LDrmDevice() override;

    void openAuto();
    bool open(const std::string &nodePath); // Opens a specific file, e.g., "/dev/dri/card0"
    void close();

    bool isOpen() const;
    int fd() const; // DRM file descriptor (very important!)

    LConnectorInfo primaryConnector() const;
    std::vector<LConnectorInfo> allConnectors() const;
    std::vector<LConnectorInfo> connectedConnectors() const;

    gbm_device *gbmDevice() const;
    std::string deviceName() const;

    std::optional<LConnectorInfo> getConnectorByName(const std::string &name) const;

protected:
    //TODO register and unregister only by pointers?
    void registerSurface(uint32_t crtcId, LScreenSurface *surface);
    void unregisterSurface(uint32_t crtcId);

    void handleEpollEvent(uint32_t events) override;

private:
    int m_fd = -1;
    gbm_device *m_gbm = nullptr;
    bool m_registeredToEpoll = false;

    // Maps CRTC ID to the corresponding Surface
    std::unordered_map<uint32_t, LScreenSurface *> m_activeSurfaces;
};