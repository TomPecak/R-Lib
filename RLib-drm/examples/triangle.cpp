#include <GLES2/gl2.h>

#include <iostream>

#include <LEventLoop.hpp>
#include <LTimer.hpp>

#include <LDrmDevice.hpp>
#include <LEglContext.hpp>
#include <LScreenSurface.hpp>

#include "linmath.h"

typedef struct Vertex
{
    vec2 pos;
    vec3 col;
} Vertex;

static const Vertex vertices[3] = {{{-0.6f, -0.4f}, {1.f, 0.f, 0.f}},
                                   {{0.6f, -0.4f}, {0.f, 1.f, 0.f}},
                                   {{0.f, 0.6f}, {0.f, 0.f, 1.f}}};

static const char *vertex_shader_text = "#version 100\n"
                                        "precision mediump float;\n"
                                        "uniform mat4 MVP;\n"
                                        "attribute vec3 vCol;\n"
                                        "attribute vec2 vPos;\n"
                                        "varying vec3 color;\n"
                                        "void main()\n"
                                        "{\n"
                                        "    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
                                        "    color = vCol;\n"
                                        "}\n";

static const char *fragment_shader_text = "#version 100\n"
                                          "precision mediump float;\n"
                                          "varying vec3 color;\n"
                                          "void main()\n"
                                          "{\n"
                                          "    gl_FragColor = vec4(color, 1.0);\n"
                                          "}\n";

int main()
{
    std::cout << "[RLib-drm][Example] Simple Triangle" << std::endl;

    LEventLoop loop;

    LDrmDevice gpu;
    gpu.openAuto();

    LConnectorInfo primaryConnector = gpu.primaryConnector();
    LScreenSurface screenSurface(&gpu, primaryConnector);
    LEglContext context;

    context.setFormat(LEglContext::OpenGLES, 2, 0);
    context.create(&gpu);
    context.makeCurrent(&screenSurface);

    //--------- OpenGl ---------------

    GLuint vertex_buffer;
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);

    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    const GLint mvp_location = glGetUniformLocation(program, "MVP");
    const GLint vpos_location = glGetAttribLocation(program, "vPos");
    const GLint vcol_location = glGetAttribLocation(program, "vCol");

    glEnableVertexAttribArray(vpos_location);
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vpos_location,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (void *) offsetof(Vertex, pos));
    glVertexAttribPointer(vcol_location,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (void *) offsetof(Vertex, col));

    double time = 0.0;
    double delta_time = 1.0 / double(primaryConnector.displayRefreshRate);

    screenSurface.onFrameReady([&]() {
        int width = screenSurface.width();
        int height = screenSurface.height();

        const float ratio = width / (float) height;

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        mat4x4 m, p, mvp;
        mat4x4_identity(m);
        mat4x4_rotate_Z(m, m, float(time));
        mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
        mat4x4_mul(mvp, p, m);

        glUseProgram(program);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat *) &mvp);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        time = time + delta_time;

        context.swap();
        screenSurface.swapBuffers();
    });

    glViewport(0, 0, screenSurface.width(), screenSurface.height());
    glClearColor(0.847f, 0.937f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    context.swap();
    screenSurface.swapBuffers();

    LTimer quitTimer;
    quitTimer.onTimeout([]() { LEventLoop::quit(); });
    quitTimer.start(60000);

    return loop.exec();
}
