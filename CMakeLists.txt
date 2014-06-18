CMAKE_MINIMUM_REQUIRED(VERSION 2.8.1 FATAL_ERROR)

PROJECT ( IMBATRACER CXX )

include(FindPkgConfig)

# Dependencies: Also let pkgconfig search in LD_LIBRARY_PATH/pkgconfig
set(PKG_PATHS "$ENV{LD_LIBRARY_PATH}")
if(NOT (PKG_PATHS STREQUAL ""))
	string(REGEX REPLACE ":" "/pkgconfig:" PKG_PATHS "${PKG_PATHS}") # append /pkgconfig to all paths but the last one
	set(PKG_PATHS "${PKG_PATHS}/pkgconfig") # also append it to the last path
	# update environment variable
	if (ENV{PKG_CONFIG_PATH} STREQUAL "")
		set(ENV{PKG_CONFIG_PATH} ":${PKG_PATHS}")
	else()
		set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${PKG_PATHS}")
	endif()
endif()
message(STATUS "pkgconfig search path: $ENV{PKG_CONFIG_PATH}")

# SDL
pkg_check_modules(SDL2 sdl2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
link_directories(${SDL2_LIBRARY_DIRS})
# PNG
#find_package(PNG REQUIRED)
#include_directories(${PNG_INCLUDE_DIRS})
#link_directories(${PNG_LIBRARY_DIRS})


set(CUDA_DIR "/opt/cuda6.0" CACHE STRING "The directory containing the CUDA toolkit")
FIND_PACKAGE ( THORIN REQUIRED )
message(STATUS "Using THORIN at ${THORIN_RUNTIME_DIR}")
message(STATUS "Using CUDA at ${CUDA_DIR}")
include(${THORIN_RUNTIME_DIR}/cmake/ThorinRuntime.cmake)
include_directories(${THORIN_RUNTIME_INCLUDE_DIR})


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")


add_subdirectory(src)


