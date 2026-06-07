#include <cstdint>
#include <iostream>

#include <LEventLoop.hpp>

#include "LDrmDevice.hpp"
#include "LEglContext.hpp"
#include "LScreenSurface.hpp"

//dummy glViewport
void glViewport(int x, int y, uint32_t width, uint32_t height) {}

int main()
{
    //LEventLoop loop;

    LDrmDevice gpu;
    //gpu.open("/dev/dri/card1");
    gpu.openAuto();

    std::cout << "Found card: " << gpu.deviceName() << std::endl;
    for (const auto &screen : gpu.connectedScreens()) {
        std::cout << "Screen: " << screen.name << " " << screen.width << "x" << screen.height << "@"
                  << screen.refreshRate << "Hz" << std::endl;
    }

    LScreenInfo primaryScreen = gpu.primaryScreen();
    std::cout << "Primary screen: " << primaryScreen.name << " " << primaryScreen.width << "x"
              << primaryScreen.height << "@" << primaryScreen.refreshRate << "Hz" << std::endl;

    std::cout << "Hello DRM!" << std::endl;
    //return loop.exec();
}

/*
sudo apt update
sudo apt install pkg-config cmake build-essential \
                 libdrm-dev libgbm-dev libegl-dev \
                 libgl-dev libgles2-dev
*/

// int main()
// {
//     std::cout << "Hello DRM!" << std::endl;

//     //Scenario: two different screens with two different opengl context

//     // 1. Hardware initialization
//     LDrmDevice gpu;
//     gpu.openAuto();

//     LScreenSurface screenMain(&gpu);
//     screenMain.setupOutput("HDMI-A-1");

//     LScreenSurface screenSecondary(&gpu);
//     screenSecondary.setupOutput("DSI-1");

//     // 2. Creating a full OpenGL context (Desktop)
//     LEglContext desktopGlContext;
//     desktopGlContext.setFormat(LEglContext::OpenGL, 4, 5); // Full OpenGL 4.5
//     desktopGlContext.create(&gpu);

//     // 3. Creating the second context - OpenGL ES (Mobile/Embedded)
//     LEglContext mobileGlesContext;
//     mobileGlesContext.setFormat(LEglContext::OpenGLES, 3, 2); // OpenGL ES 3.2
//     mobileGlesContext.create(&gpu);

//     // 4. Shared event loop
//     screenMain.onFrameReady([&]() {
//         // Activate full OpenGL for the main screen
//         desktopGlContext.makeCurrent(&screenMain);
//         glViewport(0, 0, screenMain.width(), screenMain.height());
//         // ... here we use OpenGL 4.5 specific functions (e.g., Geometry Shaders)
//         screenMain.swapBuffers();
//     });

//     screenSecondary.onFrameReady([&]() {
//         // Activate OpenGL ES for the small screen
//         mobileGlesContext.makeCurrent(&screenSecondary);
//         glViewport(0, 0, screenSecondary.width(), screenSecondary.height());
//         // ... here we use OpenGL ES functions (e.g., lighter shaders with highp/mediump precision)
//         screenSecondary.swapBuffers();
//     });
// }