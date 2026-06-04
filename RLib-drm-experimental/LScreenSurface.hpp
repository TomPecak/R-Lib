#pragma once

#include <cstdint>
#include <functional>
#include <string>

class LDrmDevice;
struct LConnectorInfo;
struct gbm_surface;
struct gbm_bo;
struct _drmModeCrtc;

class LScreenSurface
{
    //grand access to handlePageFlip() methd
    friend class LDrmDevice;

public:
    // First initialization method
    LScreenSurface(LDrmDevice *device, const LConnectorInfo &connectorInfo);
    explicit LScreenSurface(LDrmDevice *device);
    ~LScreenSurface();

    void setupOutput(const std::string &screenName);

    uint32_t width() const;
    uint32_t height() const;

    void swapBuffers();

    // Event Loop callbacks
    void onFrameReady(std::function<void()> callback);

    // Methods for EGL/Vulkan layer
    gbm_surface *nativeSurface() const;

    //Optional for Vulkan? or let Vulkan full control
    //void presentImage(uint32_t imageIndex);

    //callback from epoll drm
    void handlePageFlip();

private:
    void setupInternal(const LConnectorInfo &screenInfo);

    uint32_t getFramebufferId(gbm_bo *bo);

    LDrmDevice *m_device = nullptr;
    gbm_surface *m_gbmSurface = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    uint32_t m_connectorId = 0;
    uint32_t m_crtcId = 0;

    std::function<void()> m_frameReadyCallback;
    bool m_pageFlipPending = false;
    bool m_firstFrame = true;

    // State preservation (so we can restore the console/X11 screen on exit)
    _drmModeCrtc *m_savedCrtc = nullptr;

    // Buffer tracking structure
    struct BufferContext
    {
        gbm_bo *bo = nullptr;
        uint32_t fbId = 0;
    };

    BufferContext m_currentBuffer;
    BufferContext m_nextBuffer;
};