#include "LEglContext.hpp"
#include "LDrmDevice.hpp"
#include "LScreenSurface.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h> // Required for modern (platform-based) EGL with GBM

#include <iostream>
#include <stdexcept>
#include <vector>

// Modern Display loading function typedef
// We use PFNEGLGETPLATFORMDISPLAYEXTPROC instead of the deprecated eglGetDisplay
static PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT_ptr = nullptr;

LEglContext::LEglContext() {}

LEglContext::~LEglContext()
{
    destroy();
}

void LEglContext::setFormat(Api api, int majorVersion, int minorVersion)
{
    m_api = api;
    m_major = majorVersion;
    m_minor = minorVersion;
}

bool LEglContext::create(LDrmDevice *device)
{
    if (!device || !device->isOpen()) {
        std::cerr << "[LEglContext] [ERROR] Invalid DRM Device provided!" << std::endl;
        return false;
    }

    // 1. Locate the proper extension function to load EGL directly from GBM
    eglGetPlatformDisplayEXT_ptr = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress(
        "eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT_ptr) {
        std::cerr << "[LEglContext] [ERROR] System does not support eglGetPlatformDisplayEXT!"
                  << std::endl;
        return false;
    }

    // 2. Obtain the Display abstraction from hardware (via gbm_device)
    m_display = eglGetPlatformDisplayEXT_ptr(EGL_PLATFORM_GBM_KHR, device->gbmDevice(), nullptr);
    if (m_display == EGL_NO_DISPLAY) {
        std::cerr << "[LEglContext] [ERROR] Could not obtain EGL Display from GBM device!"
                  << std::endl;
        return false;
    }

    // 3. Initialize EGL subsystem
    EGLint eglMajor, eglMinor;
    if (!eglInitialize(m_display, &eglMajor, &eglMinor)) {
        std::cerr << "[LEglContext] [ERROR] eglInitialize failed!" << std::endl;
        return false;
    }
    std::cout << "[LEglContext] [INFO] EGL initialized successfully (Version " << eglMajor << "."
              << eglMinor << ")" << std::endl;

    // 4. Bind the correct graphical API to EGL
    if (m_api == OpenGL) {
        if (!eglBindAPI(EGL_OPENGL_API)) {
            std::cerr << "[LEglContext] [ERROR] Failed to bind OpenGL API!" << std::endl;
            return false;
        }
    } else { // OpenGLES
        if (!eglBindAPI(EGL_OPENGL_ES_API)) {
            std::cerr << "[LEglContext] [ERROR] Failed to bind OpenGL ES API!" << std::endl;
            return false;
        }
    }

    // 5. Search for an appropriate color/buffer configuration
    // For hardware DRM, the 32-bit XRGB8888 format is the most desirable (no alpha channel in the window).
    const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                    EGL_WINDOW_BIT,
                                    EGL_RED_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_ALPHA_SIZE,
                                    0,
                                    EGL_RENDERABLE_TYPE,
                                    (m_api == OpenGL) ? EGL_OPENGL_BIT : EGL_OPENGL_ES3_BIT,
                                    EGL_NONE};

    EGLint numConfigs;
    if (!eglChooseConfig(m_display, configAttribs, &m_config, 1, &numConfigs) || numConfigs != 1) {
        std::cerr << "[LEglContext] [ERROR] Failed to find a suitable EGL config!" << std::endl;
        return false;
    }

    // 6. Set up the target context attributes (versions, etc.)
    std::vector<EGLint> contextAttribs;
    contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    contextAttribs.push_back(m_major);
    contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION);
    contextAttribs.push_back(m_minor);

    if (m_api == OpenGL) {
        // Modern Core Profile for OpenGL (drops legacy compatibility baggage)
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
    }
    contextAttribs.push_back(EGL_NONE);

    // 7. Create the context
    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttribs.data());
    if (m_context == EGL_NO_CONTEXT) {
        std::cerr << "[LEglContext] [ERROR] Failed to create EGL Context!" << std::endl;
        return false;
    }

    std::cout << "[LEglContext] [SUCCESS] " << (m_api == OpenGL ? "OpenGL" : "OpenGL ES") << " "
              << m_major << "." << m_minor << " Context Created!" << std::endl;

    return true;
}

void LEglContext::destroy()
{
    if (m_display != EGL_NO_DISPLAY) {
        // We must unbind the context from the current thread before destroying it
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        m_currentAttachedScreen = nullptr; // Clear the cache

        if (m_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_display, m_context);
            m_context = EGL_NO_CONTEXT;
        }

        // Note: All created EGLSurfaces attached to LScreenSurface are automatically
        // destroyed by the EGL implementation when eglTerminate is called.
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        std::cout << "[LEglContext] [INFO] Context destroyed." << std::endl;
    }
}

bool LEglContext::makeCurrent(LScreenSurface *screen)
{
    if (!screen || !screen->nativeSurface()) {
        std::cerr << "[LEglContext] [ERROR] Cannot make current on a null/invalid screen surface."
                  << std::endl;
        return false;
    }

    // EARLY RETURN OPTIMIZATION:
    // If the requested screen is already the active one, we skip the heavy EGL state switch.
    // The "eglSurface() != nullptr" check protects us from pointer aliasing (memory reuse)
    // in case a new LScreenSurface was created at the exact same memory address as an old one.
    if (m_currentAttachedScreen == screen && screen->eglSurface() != nullptr) {
        return true;
    }

    // Cast void* back to EGLSurface
    EGLSurface screenEglSurface = static_cast<EGLSurface>(screen->eglSurface());

    // If the screen doesn't have an EGL surface created yet, we must create it once
    if (screenEglSurface == nullptr) {
        // screen->nativeSurface() returns a pointer to gbm_surface. EGL knows how to handle it.
        screenEglSurface = eglCreateWindowSurface(m_display,
                                                  m_config,
                                                  reinterpret_cast<EGLNativeWindowType>(
                                                      screen->nativeSurface()),
                                                  nullptr);

        if (screenEglSurface == EGL_NO_SURFACE) {
            std::cerr << "[LEglContext] [ERROR] Failed to create EGL Window Surface from GBM!"
                      << std::endl;
            m_currentAttachedScreen = nullptr; // Reset cache on failure
            return false;
        }

        // Save the surface inside the screen to avoid recreating it every frame!
        screen->setEglSurface(screenEglSurface);
    }

    // Bind the thread context (C and C++ are multi-threaded languages, this specifies where GL commands go)
    if (!eglMakeCurrent(m_display, screenEglSurface, screenEglSurface, m_context)) {
        std::cerr << "[LEglContext] [ERROR] eglMakeCurrent failed!" << std::endl;
        m_currentAttachedScreen = nullptr; // Reset cache on failure
        return false;
    }

    // Successfully switched the context, update our cache
    m_currentAttachedScreen = screen;
    return true;
}

void LEglContext::swap(LScreenSurface *screen)
{
    if (screen && screen->eglSurface()) {
        // This command forces the EGL driver (e.g. Mesa) to complete operations (Flush)
        // and make the front-buffer available back to GBM, from which your swapBuffers will pick the frame ID!
        eglSwapBuffers(m_display, static_cast<EGLSurface>(screen->eglSurface()));
    }
}