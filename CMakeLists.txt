project(IMBATRACER)

cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

set (CMAKE_CXX_STANDARD 14)

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-reorder -Wno-long-long -Wno-sign-compare")
endif()

# AnyDSL runtime setup
find_package(AnyDSL-runtime REQUIRED)

include(${ANYDSL_RUNTIME_CMAKE_DIR}/Runtime.cmake)
include_directories(${ANYDSL_RUNTIME_DIR}/src)

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

# OpenImageIO
find_package (OpenImageIO 1.7 REQUIRED)
include_directories ("${OPENIMAGEIO_INCLUDE_DIR}")
link_directories ("${OPENIMAGEIO_LIBRARY_DIRS}")
message (STATUS "Using OpenImageIO ${OPENIMAGEIO_VERSION}")

# OpenShadingLanguage
find_package (OpenShadingLanguage REQUIRED)
include_directories ("${OSL_INCLUDE_DIR}")
link_directories ("${OSL_LIBRARY_DIRS}")

# OSL requires RTTI to be disabled (we have to inherit from OSL classes)
if (MSVC)
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-" )
else ()
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti" )
endif()

# Add the project files
include_directories(src)
add_subdirectory(src)

