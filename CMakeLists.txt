project(IMBATRACER)

cmake_minimum_required(VERSION 3.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

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

# AnyDSL runtime
find_package(AnyDSL_runtime REQUIRED)
include_directories(${AnyDSL_runtime_INCLUDE_DIRS})

# pthreads
find_package(Threads REQUIRED)

# libpng
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

# SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

# Traversal
find_package(Traversal REQUIRED)
include_directories(${TRAVERSAL_INCLUDE_DIR})

# Intel TBB
find_package(TBB REQUIRED)
include_directories(${TBB_INCLUDE_DIRS})

# Add the project files
include_directories(src)
add_subdirectory(src)

