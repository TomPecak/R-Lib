cmake_minimum_required(VERSION 3.16)
project(RLib-3D-math LANGUAGES CXX)

# Disable GLM internal tests to reduce configuration/build time
# and prevent polluting the main project's test suite.
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(glm/)

# ==============================================================================
# INTERFACE TARGET DEFINITION
# ==============================================================================
# We define this as an INTERFACE library because GLM is a header-only library.
# This target acts as a "configuration payload" - any target linking against
# RLib::3D-math will automatically inherit its headers, compiler flags, and macros.
add_library(${PROJECT_NAME} INTERFACE)
add_library(RLib::3D-math ALIAS ${PROJECT_NAME})

# Link against the header-only target to avoid building unnecessary static libs (libglm.a).
target_link_libraries(${PROJECT_NAME} INTERFACE glm::glm-header-only)

# Require C++17 standard for any consumer of this math library.
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

# ==============================================================================
# COMPILER OPTIONS (MATH OPTIMIZATIONS)
# ==============================================================================
# Note: Global optimizations like -O3 or -flto (Link-Time Optimization) should
# be set by the top-level executable/project. We only inject math-specific flags here.
#
# -ffast-math: Trades strict IEEE 754 compliance for significantly faster math operations.
target_compile_options(${PROJECT_NAME} INTERFACE -ffast-math)

# Detect target architecture to enable specific hardware instruction sets.
# This handles both cross-compilation and native host builds.
set(TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")

if(CMAKE_CROSSCOMPILING)
    # Cross-compilation mode: explicitly set architecture flags based on the target OS/Processor.
    if(TARGET_ARCH MATCHES "^(x86_64|amd64|AMD64)$")
        target_compile_options(${PROJECT_NAME} INTERFACE -mavx2 -mfma)
    elseif(TARGET_ARCH MATCHES "^(aarch64|arm64|ARM64)$")
        target_compile_options(${PROJECT_NAME} INTERFACE -march=armv8-a+simd)
    endif()
else()
    # Host build mode: Instruct the compiler to optimize fully for the current machine's CPU.
    target_compile_options(${PROJECT_NAME} INTERFACE -march=native)
endif()

# ==============================================================================
# GLM PREPROCESSOR CONFIGURATION
# ==============================================================================
# These definitions heavily optimize how GLM behaves in the consumer's code.
target_compile_definitions(${PROJECT_NAME} INTERFACE
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES   # Forces memory alignment (16/32 bytes), critical for SIMD performance.
    GLM_FORCE_INLINE                     # Forces aggressive inlining of math functions (zero-cost abstraction).
    GLM_FORCE_EXPLICIT_CTOR              # Disables implicit conversions (e.g., int -> float) to prevent hidden performance hits.
    GLM_FORCE_SIZE_T_LENGTH              # Makes .length() return std::size_t instead of int (Modern C++ standard compliance).
)

# Explicitly enforce hardware-specific SIMD backends in GLM based on the detected architecture.
if(TARGET_ARCH MATCHES "^(x86_64|amd64|AMD64)$")
    target_compile_definitions(${PROJECT_NAME} INTERFACE GLM_FORCE_AVX2)
elseif(TARGET_ARCH MATCHES "^(aarch64|arm64|ARM64)$")
    target_compile_definitions(${PROJECT_NAME} INTERFACE GLM_FORCE_NEON)
endif()