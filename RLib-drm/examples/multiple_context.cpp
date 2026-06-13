/*
sudo apt update
sudo apt install pkg-config cmake build-essential \
                 libdrm-dev libgbm-dev libegl-dev \
                 libgl-dev libgles2-dev
*/

#include <GLES2/gl2.h>

#include <iostream>

#include <LDrmDevice.hpp>
#include <LEglContext.hpp>
#include <LScreenSurface.hpp>

int main()
{
    std::cout << "Hello DRM!" << std::endl;

    //Scenario: two different screens with two different opengl context

    // 1. Hardware initialization
    LDrmDevice gpu;
    gpu.openAuto();

    LScreenSurface screenMain(&gpu);
    screenMain.setupOutput("HDMI-A-1");

    LScreenSurface screenSecondary(&gpu);
    screenSecondary.setupOutput("DSI-1");

    // 2. Creating a full OpenGL context (Desktop)
    LEglContext desktopGlContext;
    desktopGlContext.setFormat(LEglContext::OpenGL, 4, 5); // Full OpenGL 4.5
    desktopGlContext.create(&gpu);

    // 3. Creating the second context - OpenGL ES (Mobile/Embedded)
    LEglContext mobileGlesContext;
    mobileGlesContext.setFormat(LEglContext::OpenGLES, 3, 2); // OpenGL ES 3.2
    mobileGlesContext.create(&gpu);

    // FIRST OUTPUT RENDER
    desktopGlContext.makeCurrent(&screenMain);
    screenMain.onFrameReady([&]() {
        // Activate full OpenGL for the main screen
        desktopGlContext.makeCurrent(&screenMain);

        glViewport(0, 0, screenMain.width(), screenMain.height());
        glClearColor(0.847f, 0.937f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ... here we use OpenGL 4.5 specific functions (e.g., Geometry Shaders)
        desktopGlContext.swap();
        screenMain.swapBuffers();
    });
    // Start render
    desktopGlContext.swap();
    screenMain.swapBuffers();

    // SECOND OUTPUT RENDER
    screenSecondary.onFrameReady([&]() {
        // Activate OpenGL ES for the small screen
        mobileGlesContext.makeCurrent(&screenSecondary);
        glViewport(0, 0, screenSecondary.width(), screenSecondary.height());
        // ... here we use OpenGL ES functions (e.g., lighter shaders with highp/mediump precision)
        mobileGlesContext.swap();
        screenSecondary.swapBuffers();
    });
    // Start rendering
    mobileGlesContext.swap();
    screenSecondary.swapBuffers();
}
