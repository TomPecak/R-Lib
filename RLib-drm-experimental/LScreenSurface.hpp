#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <LEventLoop.hpp>

class LDrmDevice;
struct LScreenInfo;
struct gbm_surface;

class LScreenSurface : public LEpollHandler
{
public:
    // First Initialization method
    LScreenSurface(LDrmDevice *device, const LScreenInfo &screenInfo);

    // Second Initialization method
    explicit LScreenSurface(LDrmDevice *device);
    void setupOutput(const std::string &screenName);

    // setupPrimaryOutput(); is this method really needed?

    ~LScreenSurface() override;

    uint32_t width() const;
    uint32_t height() const;

    void swapBuffers();
    //void requestFrame(); ?????

    // Event Loop callbacks
    void onFrameReady(std::function<void()> callback);

    // Methods for EGL/Vulkan layer
    gbm_surface *nativeSurface() const;

    //Optional for Vulkan? or let Vulkan full control
    //void presentImage(uint32_t imageIndex);

protected:
    void handleEpollEvent(uint32_t events) override;

private:
    void processDrmEvents();

    LDrmDevice *m_device;
    gbm_surface *m_gbmSurface = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Internal ID for KMS
    uint32_t m_connectorId = 0;
    uint32_t m_crtcId = 0;

    std::function<void()> m_frameReadyCallback;
    bool m_pageFlipPending = false;
};