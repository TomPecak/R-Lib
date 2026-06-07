#include "LScreenSurface.hpp"

#include "LDrmDevice.hpp"

LScreenSurface::LScreenSurface(LDrmDevice *device, const LScreenInfo &screenInfo) {}

LScreenSurface::LScreenSurface(LDrmDevice *device) {}

void LScreenSurface::setupOutput(const std::string &screenName) {}

LScreenSurface::~LScreenSurface() {}

uint32_t LScreenSurface::width() const
{
    return 0;
}

uint32_t LScreenSurface::height() const
{
    return 0;
}

void LScreenSurface::swapBuffers() {}

void LScreenSurface::onFrameReady(std::function<void()> callback) {}

void LScreenSurface::handleEpollEvent(uint32_t events) {}