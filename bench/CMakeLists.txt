project(IMBATRACER)

cmake_minimum_required(VERSION 2.4)

# Thorin setup
find_package(THORIN REQUIRED)

include(${THORIN_RUNTIME_DIR}/cmake/ThorinRuntime.cmake)
include_directories(${THORIN_RUNTIME_INCLUDE_DIR})

set(RTTYPE "cpu" CACHE PATH "Runtime type (cpu, avx, nvvm, cuda)")
if (RTTYPE STREQUAL "cpu" OR RTTYPE STREQUAL "avx")
    set(DEVICE "cpu")
else()
    set(DEVICE "gpu")
endif()

# Embree setup
set(EMBREE_DIR "" CACHE PATH "Embree source directory")
set(EMBREE_COMMON_DIR "${EMBREE_DIR}/common")
set(EMBREE_INCLUDE_DIR "${EMBREE_DIR}/include")
set(EMBREE_KERNELS_DIR "${EMBREE_DIR}/kernels")
set(EMBREE_KERNELS_XEON_DIR "${EMBREE_DIR}/kernels/xeon")

include_directories(${EMBREE_KERNELS_DIR} ${EMBREE_KERNELS_XEON_DIR})
include_directories(${EMBREE_INCLUDE_DIR})
include_directories(${EMBREE_COMMON_DIR})

find_library(EMBREE_SIMD_LIB NAMES simd PATHS ${EMBREE_DIR} ${EMBREE_DIR}/build)

# Add the project files
add_subdirectory(src)

