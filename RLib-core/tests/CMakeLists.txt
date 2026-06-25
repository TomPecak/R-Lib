cmake_minimum_required(VERSION 3.16)

project(RLib-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


enable_testing()

# 1. Fetch Google Test directly from GitHub
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0 # Latest stable version
)
# Forces the use of the shared runtime library on Windows. Optional on Linux, but a good practice.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(RLibTests
  test_ltimer.cpp
  test_ludpsocket.cpp
  test_ltcpsocket.cpp
  test_licmpsocket.cpp
)

target_link_libraries(RLibTests
  PRIVATE
  RLib
  GTest::gtest_main
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "AddressSanitizer enabled for R-Lib project (Debug mode)")

    # Compiler flags:
    # -fsanitize=address : enables memory error detection
    # -g : generates debug symbols (to know exactly where the error occurred)
    # -fno-omit-frame-pointer : provides clear and readable call stacks
    target_compile_options(RLibTests PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(RLibTests PRIVATE -fsanitize=address)
endif()

include(GoogleTest)
gtest_discover_tests(RLibTests)