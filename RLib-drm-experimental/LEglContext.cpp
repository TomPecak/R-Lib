#include "LEglContext.hpp"
#include "LDrmDevice.hpp"
#include "LScreenSurface.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h> // Required for modern (platform-based) EGL with GBM
#include <gbm.h>        // Wymagane dla definicji GBM_FORMAT_XRGB8888

#include <iostream>
#include <stdexcept>
#include <vector>

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

    // 5. Wyszukiwanie ZGODNEGO EGLConfig z GBM_FORMAT_XRGB8888
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
    // Najpierw pytamy EGL ile ma JAKICHKOLWIEK konfiguracji pasujących do naszych wymagań
    if (!eglChooseConfig(m_display, configAttribs, nullptr, 0, &numConfigs) || numConfigs == 0) {
        std::cerr << "[LEglContext] [ERROR] Failed to find ANY suitable EGL config!" << std::endl;
        return false;
    }

    std::vector<EGLConfig> configs(numConfigs);
    if (!eglChooseConfig(m_display, configAttribs, configs.data(), numConfigs, &numConfigs)) {
        std::cerr << "[LEglContext] [ERROR] Failed to fetch EGL configs!" << std::endl;
        return false;
    }

    // Teraz przeszukujemy wszystkie konfiguracje, szukając tej o identyfikatorze XRGB8888
    bool foundConfig = false;
    for (EGLConfig cfg : configs) {
        EGLint visualId;
        if (eglGetConfigAttrib(m_display, cfg, EGL_NATIVE_VISUAL_ID, &visualId)) {
            if (visualId == GBM_FORMAT_XRGB8888) { // To gwarantuje nam pełną zgodność!
                m_config = cfg;
                foundConfig = true;
                break;
            }
        }
    }

    if (!foundConfig) {
        std::cerr << "[LEglContext] [ERROR] Could not find EGL config strictly matching "
                     "GBM_FORMAT_XRGB8888!"
                  << std::endl;
        return false;
    }

    // 6. Set up the target context attributes (versions, etc.)
    std::vector<EGLint> contextAttribs;
    contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    contextAttribs.push_back(m_major);
    contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION);
    contextAttribs.push_back(m_minor);

    if (m_api == OpenGL) {
        // Modern Core Profile for OpenGL
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
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        m_currentAttachedScreen = nullptr;

        if (m_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_display, m_context);
            m_context = EGL_NO_CONTEXT;
        }

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

    if (m_currentAttachedScreen == screen && screen->eglSurface() != nullptr) {
        return true;
    }

    EGLSurface screenEglSurface = static_cast<EGLSurface>(screen->eglSurface());

    if (screenEglSurface == nullptr) {
        screenEglSurface = eglCreateWindowSurface(m_display,
                                                  m_config,
                                                  reinterpret_cast<EGLNativeWindowType>(
                                                      screen->nativeSurface()),
                                                  nullptr);

        if (screenEglSurface == EGL_NO_SURFACE) {
            EGLint err = eglGetError();
            std::cerr << "[LEglContext] [ERROR] Failed to create EGL Window Surface from GBM! "
                      << "EGL Error Code: 0x" << std::hex << err << std::dec << std::endl;
            m_currentAttachedScreen = nullptr;
            return false;
        }

        screen->setEglSurface(screenEglSurface);
    }

    if (!eglMakeCurrent(m_display, screenEglSurface, screenEglSurface, m_context)) {
        EGLint err = eglGetError();
        std::cerr << "[LEglContext] [ERROR] eglMakeCurrent failed! EGL Error Code: 0x" << std::hex
                  << err << std::dec << std::endl;
        m_currentAttachedScreen = nullptr;
        return false;
    }

    m_currentAttachedScreen = screen;
    return true;
}

void LEglContext::swap()
{
    if (m_currentAttachedScreen && m_currentAttachedScreen->eglSurface()) {
        eglSwapBuffers(m_display, static_cast<EGLSurface>(m_currentAttachedScreen->eglSurface()));
    } else {
        std::cerr << "[LEglContext] [WARNING] swap() called but no screen is currently attached."
                  << std::endl;
    }
}