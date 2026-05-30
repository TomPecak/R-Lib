cmake_minimum_required(VERSION 3.16)

project(samples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(ltimer_exmaple ltimer_exmaple.cpp)
add_executable(ludpsocket_udpechoserver ludpsocket_udpechoserver.cpp)
add_executable(ludpsocket_udpsender ludpsocket_udpsender.cpp)

target_link_libraries(ltimer_exmaple PRIVATE RLib)
target_link_libraries(ludpsocket_udpechoserver PRIVATE RLib)
target_link_libraries(ludpsocket_udpsender PRIVATE RLib)


if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(ltimer_exmaple PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(ltimer_exmaple PRIVATE -fsanitize=address)

    target_compile_options(ludpsocket_udpechoserver PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(ludpsocket_udpechoserver PRIVATE -fsanitize=address)

    target_compile_options(ludpsocket_udpsender PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(ludpsocket_udpsender PRIVATE -fsanitize=address)
endif()
