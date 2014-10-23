add_subdirectory(impala)

set_source_files_properties(impala/impala_interface.o
                            PROPERTIES
                            EXTERNAL_OBJECT TRUE
                            GENERATED TRUE)

# Embree include files and libraries
set(EMBREE_DIR "" CACHE PATH "Embree source directory")
set(EMBREE_COMMON_DIR "${EMBREE_DIR}/common")
set(EMBREE_INCLUDE_DIR "${EMBREE_DIR}/include")
set(EMBREE_KERNELS_DIR "${EMBREE_DIR}/kernels")
set(EMBREE_KERNELS_XEON_DIR "${EMBREE_DIR}/kernels/xeon")

include_directories(${EMBREE_KERNELS_DIR} ${EMBREE_KERNELS_XEON_DIR})
include_directories(${EMBREE_INCLUDE_DIR})
include_directories(${EMBREE_COMMON_DIR})

find_library(EMBREE_SIMD_LIB libsimd.a PATHS ${EMBREE_DIR} ${EMBREE_DIR}/build)

# Add the executable
add_executable(bench 
               impala/impala_interface.o
               main.cpp
               bench_ray_triangle.cpp
               bench_ray_box.cpp)

add_dependencies(bench impala_interface)
target_link_libraries(bench ${EMBREE_SIMD_LIB})

# Set compiler options
get_target_property(CXXFLAGS bench_ray_triangle COMPILE_FLAGS)
if(CXXFLAGS STREQUAL "COMPILER_OPTIONS-NOTFOUND")
  set(CXXFLAGS "")
else()
  set(CXXFLAGS "${TEMP} ")
endif()
set(CXXFLAGS "${CXXFLAGS} -std=c++11")

set_target_properties(bench PROPERTIES COMPILE_FLAGS ${CXXFLAGS})

