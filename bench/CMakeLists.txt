# Compile embree first
add_subdirectory(embree)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/embree)
include_directories(embree/kernels/xeon embree/kernels embree/common embree/include)

# Add executables
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
target_link_libraries(bench imba_core)
target_link_libraries(bench embree)

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

