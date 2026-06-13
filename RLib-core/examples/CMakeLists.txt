cmake_minimum_required(VERSION 3.16)

project(examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(core-01-timer ltimer_exmaple.cpp)
add_executable(core-02-udp_echo_server ludpsocket_udpechoserver.cpp)
add_executable(core-03-udp_sender ludpsocket_udpsender.cpp)
add_executable(core-04-tcp_echo_server tcp_echo_server.cpp)

target_link_libraries(core-01-timer PRIVATE RLib)
target_link_libraries(core-02-udp_echo_server PRIVATE RLib)
target_link_libraries(core-03-udp_sender PRIVATE RLib)
target_link_libraries(core-04-tcp_echo_server PRIVATE RLib)


if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(core-01-timer PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(core-01-timer PRIVATE -fsanitize=address)

    target_compile_options(core-02-udp_echo_server PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(core-02-udp_echo_server PRIVATE -fsanitize=address)

    target_compile_options(core-03-udp_sender PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(core-03-udp_sender PRIVATE -fsanitize=address)

    target_compile_options(core-04-tcp_echo_server PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(core-04-tcp_echo_server PRIVATE -fsanitize=address)
endif()
