# Get the Impala source files names
add_subdirectory(impala)
get_directory_property(IMPALA_SRCS DIRECTORY impala DEFINITION IMPALA_SRCS)

set(IMPALA_SRCS_PREFIX "")
foreach(FILE IN ITEMS ${IMPALA_SRCS})
    set(NEW_FILE "impala/${FILE}")
    list(APPEND IMPALA_SRCS_PREFIX ${NEW_FILE})
endforeach(FILE IN ${IMPALA_SRCS})

thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    RTTYPE ${RTTYPE}
                    FILES ${IMPALA_SRCS_PREFIX})

# Generate the C interface
add_custom_target(impala-interface DEPENDS ${IMPALA_SRCS_PREFIX}
                  COMMAND impala -emit-c-interface ${IMPALA_SRCS} ${THORIN_RUNTIME_DIR}/platforms/intrinsics_cpu.impala ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala -o impala_interface
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/impala)
set_source_files_properties(impala/impala_interface.h PROPERTIES GENERATED TRUE)

# Add executables
add_executable(bench
               ${IMPALA_OBJS}
               ${EMBREE_SRCS}
               bench.cpp
               scene/scene.cpp
               loaders/obj_loader.cpp
               bench/bench_bvh_build.cpp
               bench/bench_ray_triangle.cpp
               bench/bench_ray_box.cpp
               bench/bench_ray_bvh.cpp)

include_directories(${THORIN_RUNTIME_INCLUDE_DIR})
target_link_libraries(bench ${EMBREE_LIB} ${EMBREE_AVX_LIB} ${EMBREE_AVX2_LIB} ${EMBREE_SSE41_LIB} ${EMBREE_SSE42_LIB} ${EMBREE_SIMD_LIB} ${EMBREE_SYS_LIB})
target_link_libraries(bench ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(bench ${IMPALA_LIBS})
add_dependencies(bench impala-interface)

add_executable(imbatracer
               ${IMPALA_OBJS}
               main.cpp
               scene/scene.cpp
               scene/render.cpp
               loaders/png_loader.cpp
               loaders/obj_loader.cpp
               devices/png_device.cpp
               devices/sdl_device.cpp)

include_directories(${PNG_INCLUDE_DIRS})
target_link_libraries(imbatracer ${PNG_LIBRARIES})
target_link_libraries(imbatracer ${SDL_LIBRARY})
add_dependencies(imbatracer impala-interface)

# Set compiler options
get_target_property(CXX_FLAGS bench COMPILE_FLAGS)
if(CXX_FLAGS STREQUAL "CXX_FLAGS-NOTFOUND")
  set(CXX_FLAGS "")
endif()
set(CXX_FLAGS "${CXX_FLAGS} -std=c++11")
set_target_properties(bench PROPERTIES COMPILE_FLAGS ${CXX_FLAGS})

get_target_property(CXX_FLAGS bench COMPILE_FLAGS)
if(CXX_FLAGS STREQUAL "CXX_FLAGS-NOTFOUND")
  set(CXX_FLAGS "")
endif()
set(CXX_FLAGS "${CXX_FLAGS} -std=c++11")
set_target_properties(imbatracer PROPERTIES COMPILE_FLAGS ${CXX_FLAGS})
