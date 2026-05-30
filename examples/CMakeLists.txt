cmake_minimum_required(VERSION 3.16)

project(samples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(ltimer_exmaple
    ltimer_exmaple.cpp
)
add_executable(ludpsocket_udpechoserver
    ludpsocket_udpechoserver.cpp
)
add_executable(ludpsocket_udpsender
    ludpsocket_udpsender.cpp
)



if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(R-Lib PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)

    # Linker flags (the linker also needs to be aware of the sanitizer)
    target_link_options(R-Lib PRIVATE -fsanitize=address)
endif()


include(GNUInstallDirs)
install(TARGETS R-Lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)