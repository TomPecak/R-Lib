#pragma once

class LDrmDevice;
class LScreenSurface;

typedef void *EGLDisplay;
typedef void *EGLContext;
typedef void *EGLConfig; // Needed for validation!

class LEglContext
{
public:
    enum Api { OpenGL, OpenGLES };

    LEglContext();
    ~LEglContext();

    void setFormat(Api api, int majorVersion, int minorVersion);

    // Creates the main context for a given graphics driver
    bool create(LDrmDevice *device);
    void destroy();

    // Makes the current thread and context draw to the given screen
    bool makeCurrent(LScreenSurface *screen);

    // Flushes the rendered EGL frame to the GBM buffer pool
    void swap();

private:
    Api m_api = OpenGLES;
    int m_major = 3;
    int m_minor = 0;

    EGLDisplay m_display = nullptr;
    EGLContext m_context = nullptr;
    EGLConfig m_config = nullptr;

    // Tracks the currently bound surface to prevent redundant eglMakeCurrent calls
    LScreenSurface *m_currentAttachedScreen = nullptr;
};