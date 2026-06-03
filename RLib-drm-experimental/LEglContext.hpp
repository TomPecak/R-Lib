#pragma once

class LDrmDevice;
class LScreenSurface;
typedef void *EGLDisplay;
typedef void *EGLContext;
typedef void *EGLSurface;

class LEglContext
{
public:
    enum Api { OpenGL, OpenGLES };

    LEglContext();
    ~LEglContext();

    void setFormat(Api api, int majorVersion, int minorVersion);

    bool create(LDrmDevice *device); //is this method really needed?
    // Initializes the context for a given surface
    bool create(LScreenSurface *surface);
    void destroy();

    bool makeCurrent(LScreenSurface *screen);

    // Needed for LScreenSurface::swapBuffers, although it is often integrated
    void swap();

private:
    Api m_api = OpenGLES;
    int m_major = 3;
    int m_minor = 0;

    EGLDisplay m_display = nullptr;
    EGLContext m_context = nullptr;
    EGLSurface m_surface = nullptr;
    LScreenSurface *m_targetSurface = nullptr;
};