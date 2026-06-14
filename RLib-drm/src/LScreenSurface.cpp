#include "LScreenSurface.hpp"
#include "LDrmDevice.hpp"

#include <gbm.h>
#include <iostream>
#include <stdexcept>
#include <xf86drm.h>
#include <xf86drmMode.h>

LScreenSurface::LScreenSurface(LDrmDevice *device, const LConnectorInfo &screenInfo)
    : m_device(device)
{
    std::cout << "[LScreenSurface] [INFO] Initializing surface using ConnectorInfo..." << std::endl;
    setupInternal(screenInfo);
}

LScreenSurface::LScreenSurface(LDrmDevice *device)
    : m_device(device)
{}

void LScreenSurface::setupOutput(const std::string &screenName)
{
    std::cout << "[LScreenSurface] [INFO] Attempting to setup output for: " << screenName
              << std::endl;
    if (!m_device || !m_device->isOpen()) {
        std::cerr << "[LScreenSurface] [ERROR] Invalid or closed DRM device." << std::endl;
        return;
    }

    auto connOpt = m_device->getConnectorByName(screenName);
    if (connOpt.has_value()) {
        setupInternal(connOpt.value());
    } else {
        std::cerr << "[LScreenSurface] [ERROR] Output " << screenName << " not found." << std::endl;
    }
}

void LScreenSurface::setupInternal(const LConnectorInfo &screenInfo)
{
    if (!screenInfo.connected) {
        std::cerr << "[LScreenSurface] [ERROR] Cannot setup surface on disconnected output."
                  << std::endl;
        return;
    }

    m_connectorId = screenInfo.connectorId;
    m_width = screenInfo.displayWidth;
    m_height = screenInfo.displayHeight;

    int fd = m_device->fd();
    drmModeRes *res = drmModeGetResources(fd);
    drmModeConnector *conn = drmModeGetConnector(fd, m_connectorId);

    // 1. Find a suitable CRTC (Display Controller) for this connector
    if (conn->encoder_id) {
        drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
        if (enc) {
            m_crtcId = enc->crtc_id;
            drmModeFreeEncoder(enc);
        }
    }

    // If no CRTC is assigned, find the first available one
    if (m_crtcId == 0) {
        for (int i = 0; i < conn->count_encoders; i++) {
            drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[i]);
            if (!enc)
                continue;

            for (int j = 0; j < res->count_crtcs; j++) {
                if (enc->possible_crtcs & (1 << j)) {
                    m_crtcId = res->crtcs[j];
                    drmModeFreeEncoder(enc);
                    break;
                }
            }
            if (m_crtcId != 0)
                break;
        }
    }

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    if (m_crtcId == 0) {
        throw std::runtime_error("Could not find a suitable CRTC for the connector.");
    }

    // Save the original CRTC state to restore it on exit (polite to the system console)
    m_savedCrtc = drmModeGetCrtc(fd, m_crtcId);

    // 2. Create the GBM Surface
    // GBM_FORMAT_XRGB8888 is widely supported. GBM_BO_USE_SCANOUT implies it will be used for screen display.
    m_gbmSurface = gbm_surface_create(m_device->gbmDevice(),
                                      m_width,
                                      m_height,
                                      GBM_FORMAT_XRGB8888,
                                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!m_gbmSurface) {
        throw std::runtime_error("Failed to create GBM surface.");
    }

    // Register inside device to receive PageFlip events
    m_device->registerSurface(m_crtcId, this);

    std::cout << "[LScreenSurface] [SUCCESS] Output configured. CRTC: " << m_crtcId
              << ", Resolution: " << m_width << "x" << m_height << std::endl;
}

LScreenSurface::~LScreenSurface()
{
    if (m_device && m_crtcId != 0) {
        // Restore previous screen state
        if (m_savedCrtc) {
            std::cout << "[LScreenSurface] [INFO] Restoring original CRTC state..." << std::endl;
            drmModeSetCrtc(m_device->fd(),
                           m_savedCrtc->crtc_id,
                           m_savedCrtc->buffer_id,
                           m_savedCrtc->x,
                           m_savedCrtc->y,
                           &m_connectorId,
                           1,
                           &m_savedCrtc->mode);
            drmModeFreeCrtc(m_savedCrtc);
        }
        m_device->unregisterSurface(m_crtcId);
    }

    if (m_currentBuffer.bo)
        gbm_surface_release_buffer(m_gbmSurface, m_currentBuffer.bo);
    if (m_nextBuffer.bo)
        gbm_surface_release_buffer(m_gbmSurface, m_nextBuffer.bo);
    if (m_gbmSurface)
        gbm_surface_destroy(m_gbmSurface);
}

uint32_t LScreenSurface::width() const
{
    return m_width;
}
uint32_t LScreenSurface::height() const
{
    return m_height;
}
gbm_surface *LScreenSurface::nativeSurface() const
{
    return m_gbmSurface;
}
void LScreenSurface::onFrameReady(std::function<void()> callback)
{
    m_frameReadyCallback = callback;
}

// Helper function: Maps a GBM buffer to a DRM Framebuffer ID
uint32_t LScreenSurface::getFramebufferId(gbm_bo *bo)
{
    if (!bo)
        return 0;

    // Check if we already created an FB for this BO (GBM allows setting custom user data per BO)
    uint32_t fbId = reinterpret_cast<intptr_t>(gbm_bo_get_user_data(bo));
    if (fbId != 0)
        return fbId;

    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;

    int ret = drmModeAddFB(m_device->fd(), width, height, 24, 32, stride, handle, &fbId);
    if (ret != 0) {
        std::cerr << "[LScreenSurface] [ERROR] Failed to add DRM Framebuffer." << std::endl;
        return 0;
    }

    // Attach the ID so we don't create it again
    gbm_bo_set_user_data(bo, reinterpret_cast<void *>(static_cast<intptr_t>(fbId)), nullptr);
    return fbId;
}

void LScreenSurface::swapBuffers()
{
    if (m_pageFlipPending) {
        std::cerr << "[LScreenSurface] [WARNING] Swap called while page flip is still pending. "
                     "Dropping frame."
                  << std::endl;
        return;
    }

    // Lock the front buffer so we can present it.
    // NOTE: This assumes that EGL/Vulkan has already rendered into it!
    m_nextBuffer.bo = gbm_surface_lock_front_buffer(m_gbmSurface);
    if (!m_nextBuffer.bo) {
        std::cerr << "[LScreenSurface] [ERROR] Failed to lock GBM front buffer!" << std::endl;
        return;
    }

    m_nextBuffer.fbId = getFramebufferId(m_nextBuffer.bo);

    if (m_firstFrame) {
        // DRM rule: The first frame MUST be presented synchronously using SetCrtc
        drmModeModeInfo mode;
        auto connectors = m_device->connectedConnectors();
        for (const auto &c : connectors) {
            if (c.connectorId == m_connectorId && !c.availableModes.empty()) {
                drmModeRes *res = drmModeGetResources(m_device->fd());
                drmModeConnector *conn = drmModeGetConnector(m_device->fd(), m_connectorId);
                mode = conn->modes[0]; // Assuming native
                drmModeFreeConnector(conn);
                drmModeFreeResources(res);
                break;
            }
        }

        int ret = drmModeSetCrtc(m_device->fd(),
                                 m_crtcId,
                                 m_nextBuffer.fbId,
                                 0,
                                 0,
                                 &m_connectorId,
                                 1,
                                 &mode);
        if (ret == 0) {
            m_firstFrame = false;
            // Since it was synchronous, we simulate a page flip completion instantly
            handlePageFlip();
        } else {
            std::cerr << "[LScreenSurface] [ERROR] Initial drmModeSetCrtc failed!" << std::endl;
        }
    } else {
        // Asynchronous Page Flip for smooth vsync
        int ret = drmModePageFlip(m_device->fd(),
                                  m_crtcId,
                                  m_nextBuffer.fbId,
                                  DRM_MODE_PAGE_FLIP_EVENT,
                                  this);
        if (ret == 0) {
            m_pageFlipPending = true;
        } else {
            std::cerr << "[LScreenSurface] [ERROR] drmModePageFlip failed!" << std::endl;
        }
    }
}

void LScreenSurface::handlePageFlip()
{
    // The kernel has successfully displayed m_nextBuffer.
    // We can now release the old buffer back to the GBM pool.
    if (m_currentBuffer.bo) {
        gbm_surface_release_buffer(m_gbmSurface, m_currentBuffer.bo);
    }

    m_currentBuffer = m_nextBuffer;
    m_pageFlipPending = false;

    // Notify the application event loop that it's safe to render the next frame
    if (m_frameReadyCallback) {
        m_frameReadyCallback();
    }
}