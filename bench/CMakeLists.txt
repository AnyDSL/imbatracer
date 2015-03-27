# Compile embree first
add_subdirectory(embree)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/embree)
include_directories(embree/kernels/xeon embree/kernels embree/common embree/include)

# Get the Impala source files names
add_subdirectory(../src/impala impala)
get_directory_property(IMPALA_SRCS       DIRECTORY ../src/impala DEFINITION IMPALA_SRCS)
get_directory_property(IMPALA_BENCH_SRCS DIRECTORY ../src/impala DEFINITION IMPALA_BENCH_SRCS)

# Add a directory prefix to the impala files
function(ADD_PREFIX list prefix_list prefix)
    set(new_list "")
    foreach(ITEM IN ITEMS ${list})
        set(NEW_ITEM "${prefix}${ITEM}")
        list(APPEND new_list ${NEW_ITEM})
    endforeach(ITEM IN ITEMS ${list})
    set(${prefix_list} "${new_list}" PARENT_SCOPE)
endfunction(ADD_PREFIX)

add_prefix("${IMPALA_SRCS}"       IMPALA_SRCS_PREFIX       "${CMAKE_SOURCE_DIR}/src/impala/")
add_prefix("${IMPALA_BENCH_SRCS}" IMPALA_BENCH_SRCS_PREFIX "${CMAKE_SOURCE_DIR}/src/impala/")

# Generate benchmark library
thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BENCH_BACKEND}
                    FILES ${IMPALA_SRCS_PREFIX} ${IMPALA_BENCH_SRCS_PREFIX})

add_library(imba_bench
            ${IMPALA_OBJS}
            ../src/scene/scene.cpp
            ../src/scene/render.cpp)
target_link_libraries(imba_bench ${IMPALA_LIBS})

add_dependencies(imba_bench impala_interface)

# Add executable
add_executable(bench
               bench.cpp
               bench.hpp
               bench_bvh_build.cpp
               bench_bvh_build.hpp
               bench_ray_triangle.cpp
               bench_ray_triangle.hpp
               bench_ray_box.cpp
               bench_ray_box.hpp
               bench_ray_bvh.cpp
               bench_ray_bvh.hpp)

target_link_libraries(bench imba_loaders)
target_link_libraries(bench imba_bench)
target_link_libraries(bench imba_core)
target_link_libraries(bench embree)

include_directories(bench ../src)

# Set compiler options
function(ADD_CXX_FLAG target opt)
    get_target_property(CXX_FLAGS ${target} COMPILE_FLAGS)
    if(CXX_FLAGS STREQUAL "CXX_FLAGS-NOTFOUND")
      set(CXX_FLAGS "")
    endif()
    set(CXX_FLAGS "${CXX_FLAGS} ${opt}")
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${CXX_FLAGS})
endfunction(ADD_CXX_FLAG)

add_cxx_flag(bench "-std=c++11")
add_cxx_flag(imba_bench "-std=c++11")

