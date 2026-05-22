cmake_minimum_required(VERSION 3.16)

project(R-Lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(R-Lib main.cpp
    lib/LTimer.hpp
    lib/LTimer.cpp
    lib/LEventLoop.hpp
    lib/LEventLoop.cpp)

include(GNUInstallDirs)
install(TARGETS R-Lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
