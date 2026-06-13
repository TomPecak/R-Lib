cmake_minimum_required(VERSION 3.16)

project(RLib-drm-examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(drm-01-simplest_drm_app simplest_drm_app.cpp)
add_executable(drm-02-connectors_info connectors_info.cpp)
add_executable(drm-03-frist_render first_render.cpp)
add_executable(drm-04-triangle triangle.cpp)
add_executable(drm-05-multiple_gl_context multiple_context.cpp)

target_link_libraries(drm-01-simplest_drm_app PRIVATE RLib RLib-drm)
target_link_libraries(drm-02-connectors_info PRIVATE RLib RLib-drm)
target_link_libraries(drm-03-frist_render PRIVATE RLib RLib-drm)
target_link_libraries(drm-04-triangle PRIVATE RLib RLib-drm)
target_link_libraries(drm-05-multiple_gl_context PRIVATE RLib RLib-drm)


if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(drm-01-simplest_drm_app PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(drm-01-simplest_drm_app PRIVATE -fsanitize=address)

    target_compile_options(drm-02-connectors_info PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(drm-02-connectors_info PRIVATE -fsanitize=address)

    target_compile_options(drm-03-frist_render PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(drm-03-frist_render PRIVATE -fsanitize=address)

    target_compile_options(drm-04-triangle PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(drm-04-triangle PRIVATE -fsanitize=address)

    target_compile_options(drm-05-multiple_gl_context PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(drm-05-multiple_gl_context PRIVATE -fsanitize=address)

endif()