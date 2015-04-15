project(IMBATRACER)

cmake_minimum_required(VERSION 2.4)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

# Thorin setup
find_package(THORIN REQUIRED)

include(${THORIN_RUNTIME_DIR}/cmake/ThorinRuntime.cmake)
include_directories(${THORIN_RUNTIME_INCLUDE_DIR})

if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
    set(BACKEND "cpu")
else()
    set(BACKEND "cuda")
endif()

if(NOT BENCH_BACKEND)
    set(BENCH_BACKEND ${BACKEND} CACHE STRING "bench backend")
endif()

# pthreads
find_package(Threads REQUIRED)

# libpng
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

# SDL
find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

# Add the project files
add_subdirectory(src)

include_directories(src)
add_subdirectory(bench)

