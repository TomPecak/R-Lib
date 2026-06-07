#include "LEglContext.hpp"

LEglContext::LEglContext() {}

LEglContext::~LEglContext() {}

void LEglContext::setFormat(Api api, int majorVersion, int minorVersion) {}

bool LEglContext::create(LDrmDevice *device)
{
    // // 1. Before creating the context, you must inform EGL which API you are targeting
    // if (m_api == OpenGL) {
    //     eglBindAPI(EGL_OPENGL_API);
    // } else if (m_api == OpenGLES) {
    //     eglBindAPI(EGL_OPENGL_ES_API);
    // }

    // // 2. Configure context attributes
    // std::vector<EGLint> contextAttribs;
    // contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    // contextAttribs.push_back(m_major);
    // contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION);
    // contextAttribs.push_back(m_minor);

    // if (m_api == OpenGL) {
    //     // For modern full OpenGL, we usually want the Core Profile
    //     contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
    //     contextAttribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
    // }
    // contextAttribs.push_back(EGL_NONE);

    // // 3. Create the context
    // m_context = eglCreateContext(device->eglDisplay(), m_eglConfig, EGL_NO_CONTEXT, contextAttribs.data());

    // return m_context != EGL_NO_CONTEXT;
    return true;
}

bool LEglContext::makeCurrent(LScreenSurface *screen)
{
    // eglMakeCurrent(m_display, surface->nativeSurface(), surface->nativeSurface(), m_context); ???
    return true;
}