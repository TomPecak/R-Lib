#include "LDrmDevice.hpp"

#include <fcntl.h>
#include <gbm.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <iostream>

// Helper function converting DRM constants to readable strings
static std::string getConnectorName(drmModeConnector *conn)
{
    std::string typeName;
    switch (conn->connector_type) {
    case DRM_MODE_CONNECTOR_Unknown:
        typeName = "Unknown";
        break;
    case DRM_MODE_CONNECTOR_VGA:
        typeName = "VGA";
        break;
    case DRM_MODE_CONNECTOR_DVII:
        typeName = "DVI-I";
        break;
    case DRM_MODE_CONNECTOR_DVID:
        typeName = "DVI-D";
        break;
    case DRM_MODE_CONNECTOR_DVIA:
        typeName = "DVI-A";
        break;
    case DRM_MODE_CONNECTOR_Composite:
        typeName = "Composite";
        break;
    case DRM_MODE_CONNECTOR_SVIDEO:
        typeName = "SVIDEO";
        break;
    case DRM_MODE_CONNECTOR_LVDS:
        typeName = "LVDS";
        break;
    case DRM_MODE_CONNECTOR_Component:
        typeName = "Component";
        break;
    case DRM_MODE_CONNECTOR_9PinDIN:
        typeName = "DIN";
        break;
    case DRM_MODE_CONNECTOR_DisplayPort:
        typeName = "DP";
        break;
    case DRM_MODE_CONNECTOR_HDMIA:
        typeName = "HDMI-A";
        break;
    case DRM_MODE_CONNECTOR_HDMIB:
        typeName = "HDMI-B";
        break;
    case DRM_MODE_CONNECTOR_TV:
        typeName = "TV";
        break;
    case DRM_MODE_CONNECTOR_eDP:
        typeName = "eDP";
        break;
    case DRM_MODE_CONNECTOR_VIRTUAL:
        typeName = "Virtual";
        break;
    case DRM_MODE_CONNECTOR_DSI:
        typeName = "DSI";
        break;
    case DRM_MODE_CONNECTOR_DPI:
        typeName = "DPI";
        break;
    case DRM_MODE_CONNECTOR_WRITEBACK:
        typeName = "Writeback";
        break;
    case DRM_MODE_CONNECTOR_SPI:
        typeName = "SPI";
        break;
    case DRM_MODE_CONNECTOR_USB:
        typeName = "USB";
        break;
    default:
        typeName = "Other";
        break;
    }

    // Returns e.g., "HDMI-A-1", "DPI-1", "SPI-1"
    return typeName + "-" + std::to_string(conn->connector_type_id);
}

LDrmDevice::LDrmDevice()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

LDrmDevice::~LDrmDevice()
{
    close();
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

void LDrmDevice::openAuto()
{
    std::cout << "[LDrmDevice] [INFO] Starting auto-detection of DRM devices..." << std::endl;

    // On Linux systems, graphics card devices are usually located
    // at paths from /dev/dri/card0 to /dev/dri/card63.
    for (int i = 0; i < 64; ++i) {
        std::string path = "/dev/dri/card" + std::to_string(i);

        if (open(path)) {
            // Since we successfully opened the file and bound GBM,
            // we assume a suitable graphics card has been found.
            std::cout << "[LDrmDevice] [SUCCESS] Auto-detection finished. Using device: " << path
                      << std::endl;
            return;
        }
    }

    std::cerr << "[LDrmDevice] [ERROR] No compatible DRM device found among /dev/dri/card0-63."
              << std::endl;
}

bool LDrmDevice::open(const std::string &nodePath)
{
    std::cout << "[LDrmDevice] [INFO] Attempting to open node: " << nodePath << std::endl;
    close();

    // Open the device with Read/Write and Close-on-Exec flags
    m_fd = ::open(nodePath.c_str(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        // Logged as DEBUG because during openAuto() it's normal that most cards don't exist
        std::cout << "[LDrmDevice] [DEBUG] Failed to open file descriptor for " << nodePath
                  << std::endl;
        return false;
    }

    // Check if the device supports KMS (Kernel Mode Setting).
    // If we cannot retrieve resources, it means this is just a render node (e.g., renderD128)
    // and it cannot handle hardware screen display.
    drmModeRes *resources = drmModeGetResources(m_fd);
    if (!resources) {
        std::cout << "[LDrmDevice] [DEBUG] Device " << nodePath
                  << " opened, but does not support KMS (no resources). Skipping." << std::endl;
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    drmModeFreeResources(resources);

    // Initialize GBM - kernel mechanism for allocating graphics memory
    // for EGL/Vulkan without the need for X11/Wayland.
    m_gbm = gbm_create_device(m_fd);
    if (!m_gbm) {
        std::cerr << "[LDrmDevice] [ERROR] Failed to create GBM device for " << nodePath
                  << std::endl;
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    std::cout << "[LDrmDevice] [INFO] Successfully opened and initialized DRM/GBM on: " << nodePath
              << std::endl;
    return true;
}

void LDrmDevice::close()
{
    if (m_gbm) {
        gbm_device_destroy(m_gbm);
        std::cout << "[LDrmDevice] [INFO] Destroy GBM" << std::endl;
        m_gbm = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        std::cout << "[LDrmDevice] [INFO] Close dri device" << std::endl;
        m_fd = -1;
    }
}

bool LDrmDevice::isOpen() const
{
    return m_fd >= 0 && m_gbm != nullptr;
}

int LDrmDevice::fd() const
{
    return m_fd;
}

gbm_device *LDrmDevice::gbmDevice() const
{
    return m_gbm;
}

std::string LDrmDevice::deviceName() const
{
    if (m_fd < 0)
        return "";

    drmVersion *version = drmGetVersion(m_fd);
    if (!version)
        return "Unknown";

    std::string name = version->name;
    drmFreeVersion(version);
    return name;
}

std::vector<LScreenInfo> LDrmDevice::allScreens() const
{
    std::vector<LScreenInfo> screens;
    if (!isOpen())
        return screens;

    // Retrieve information about the cards (encoders, CRTCs, connectors)
    drmModeRes *resources = drmModeGetResources(m_fd);
    if (!resources)
        return screens;

    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnector *connector = drmModeGetConnector(m_fd, resources->connectors[i]);
        if (!connector)
            continue;

        LScreenInfo info;
        info.connectorId = connector->connector_id;
        info.name = getConnectorName(connector);
        info.connected = (connector->connection == DRM_MODE_CONNECTED);
        info.physicalWidthMm = connector->mmWidth;
        info.physicalHeightMm = connector->mmHeight;

        // If the display is connected, look for the resolution
        if (info.connected && connector->count_modes > 0) {
            // DRM returns modes sorted so that the first one (index 0)
            // is usually the native (best) resolution.
            const drmModeModeInfo &mode = connector->modes[0];
            info.width = mode.hdisplay;
            info.height = mode.vdisplay;
            info.refreshRate = mode.vrefresh;
        }

        screens.push_back(info);
        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);
    return screens;
}

std::vector<LScreenInfo> LDrmDevice::connectedScreens() const
{
    auto all = allScreens();
    std::vector<LScreenInfo> connected;
    for (const auto &screen : all) {
        if (screen.connected) {
            connected.push_back(screen);
        }
    }
    return connected;
}

LScreenInfo LDrmDevice::primaryScreen() const
{
    auto screens = connectedScreens();
    if (screens.empty()) {
        return LScreenInfo{};
    }

    // First, we look for built-in screens (usually the main ones in laptops/tablets)
    for (const auto &screen : screens) {
        if (screen.name.find("eDP") != std::string::npos
            || screen.name.find("DSI") != std::string::npos
            || screen.name.find("LVDS") != std::string::npos) {
            return screen;
        }
    }

    // If there is no internal screen, return the first connected one (e.g., HDMI connected to an RPi)
    return screens.front();
}