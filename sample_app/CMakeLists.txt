cmake_minimum_required(VERSION 3.16)

project(R-Lib-sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(R-Lib-sample-app main.cpp)
target_link_libraries(R-Lib-sample-app PRIVATE RLib)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(R-Lib-sample-app PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(R-Lib-sample-app PRIVATE -fsanitize=address)
endif()
