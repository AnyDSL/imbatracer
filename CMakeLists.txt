CMAKE_MINIMUM_REQUIRED(VERSION 2.8.1 FATAL_ERROR)

PROJECT ( IMBATRACER CXX )

include(cmake/ThorinRuntime.cmake)

include_directories(${THORIN_INCLUDE_DIRS})
link_directories(${THORIN_LIBRARY_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

IMPALA_WRAP(IMPALA_FILES stencil_int.impala)

add_executable(main ${IMPALA_FILES})
target_link_libraries(main ${THORIN_LIBRARIES})

