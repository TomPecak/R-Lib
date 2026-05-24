cmake_minimum_required(VERSION 3.16)

project(R-Lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(R-Lib main.cpp
    src/LTimer.hpp
    src/LTimer.cpp
    src/LEventLoop.hpp
    src/LEventLoop.cpp
    src/LGpioPin.hpp
    src/LGpioPin.cpp)

include(GNUInstallDirs)
install(TARGETS R-Lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
