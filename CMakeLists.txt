cmake_minimum_required(VERSION 3.16)

project(R-Lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(RLib STATIC
    src/LTimer.cpp
    src/LEventLoop.cpp
    src/LGpioPin.cpp
    src/LUdpSocket.cpp
    src/LTcpSocket.cpp
)

target_include_directories(RLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)


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

    target_compile_options(RLib PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
endif()


include(GNUInstallDirs)
install(TARGETS RLib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(examples/)

add_subdirectory(tests/)
