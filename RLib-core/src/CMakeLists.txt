cmake_minimum_required(VERSION 3.16)

project(RLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(RLib STATIC
    LByteRingBuffer.hpp
    LTimer.hpp
    LEventLoop.hpp
    LGpioPin.hpp
    LNativeTcpSocket.hpp
    LTcpServer.hpp
    LTcpSocket.hpp
    LTaskQueue.hpp
    LUnbufferedTcpSocket.hpp
    LUdpSocket.hpp
    LIcmpSocket.hpp

    LByteRingBuffer.cpp
    LTimer.cpp
    LEventLoop.cpp
    LGpioPin.cpp
    LNativeTcpSocket.cpp
    LTcpServer.cpp
    LTcpSocket.cpp
    LTaskQueue.cpp
    LUnbufferedTcpSocket.cpp
    LUdpSocket.cpp
    LIcmpSocket.cpp
)

target_include_directories(RLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(RLib PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
endif()


include(GNUInstallDirs)
install(TARGETS RLib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
