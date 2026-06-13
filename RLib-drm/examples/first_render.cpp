#include <cmath>

#include <cstdint>
#include <iostream>

#include <LEventLoop.hpp>
#include <LTimer.hpp>

#include "LDrmDevice.hpp"
#include "LEglContext.hpp"
#include "LScreenSurface.hpp"

#include <GL/gl.h>

int main()
{
    LEventLoop loop;

    // Open GPU
    LDrmDevice gpu;
    gpu.openAuto();

    // Get primary connector
    LConnectorInfo primaryConnector = gpu.primaryConnector();

    // Create GBM buffers
    LScreenSurface screenSurface(&gpu, primaryConnector);

    // Create OpenGL context
    LEglContext context;
    context.setFormat(LEglContext::OpenGL, 4, 0);
    context.create(&gpu);
    context.makeCurrent(&screenSurface);

    // Set V-SYNC callback
    screenSurface.onFrameReady([&]() {
        // Render frame
        glViewport(0, 0, screenSurface.width(), screenSurface.height());
        static float time = 0.0f;
        time += 0.01f;
        float r = 0.5f + 0.5f * sinf(time);
        float g = 0.5f + 0.5f * sinf(time + 2.0f);
        glClearColor(r, g, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        context.swap();
        screenSurface.swapBuffers();
    });

    glViewport(0, 0, screenSurface.width(), screenSurface.height());
    glClearColor(0.847f, 0.937f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Start rendering
    context.swap();
    screenSurface.swapBuffers();

    // Quit timer
    LTimer quitTimer;
    quitTimer.onTimeout([]() { LEventLoop::quit(); });
    quitTimer.start(10000);

    return loop.exec();
}
