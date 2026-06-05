cmake_minimum_required(VERSION 3.16)

project(RLib-drm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Install dependecies
# sudo apt update
# sudo apt install pkg-config cmake build-essential libdrm-dev libgbm-dev libegl-dev libgl-dev libgles2-dev

# Add user to special groups:
# sudo usermod -aG video,render $USER

find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(GBM REQUIRED gbm)

find_package(OpenGL REQUIRED COMPONENTS EGL OpenGL GLES2 GLES3)

add_executable(RLib-drm-app main.cpp
    LScreenSurface.hpp
    LScreenSurface.cpp
    LDrmDevice.hpp
    LDrmDevice.cpp
    LVulkanCore.hpp
    LEglContext.hpp
    LEglContext.cpp
    LVulkanContext.hpp)

target_include_directories(RLib-drm-app PRIVATE
    ${DRM_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
)

target_link_libraries(RLib-drm-app PRIVATE
    RLib

    ${DRM_LIBRARIES}
    ${GBM_LIBRARIES}
    OpenGL::EGL
    OpenGL::GLES2
    OpenGL::GLES3
    OpenGL::OpenGL
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(RLib-drm-app PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(RLib-drm-app PRIVATE -fsanitize=address)
endif()
