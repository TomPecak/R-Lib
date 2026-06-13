#include <iostream>

#include <LDrmDevice.hpp>
#include <LEglContext.hpp>
#include <LScreenSurface.hpp>

int main()
{
    // 1. OPEN DEFAULT GPU
    LDrmDevice gpu;
    gpu.openAuto();

    // 2. GET GPU PRIMARY CONNECTOR
    LConnectorInfo primaryConnector = gpu.primaryConnector();

    // 3. CREATE BUFFERS FOR CONNECTOR
    LScreenSurface screenSurface(&gpu, primaryConnector);

    // 4. CREATE OpenGL MACHINE ON GPU
    LEglContext context;
    context.setFormat(LEglContext::OpenGLES, 3, 0);
    context.create(&gpu);

    // 5. BIND THE CONTEXT TO THE SCREEN SURFACE FOR DRAWING
    context.makeCurrent(&screenSurface);

    return 0;
}
