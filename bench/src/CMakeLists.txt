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

# Generate the core library
add_library(imba_core
            ${IMPALA_OBJS}
            ${IMPALA_LIBS}
            scene/scene.cpp
            scene/render.cpp)

add_library(imba_devices
            devices/png_device.cpp
            devices/sdl_device.cpp)

add_library(imba_loaders
            loaders/png_loader.cpp
            loaders/obj_loader.cpp)

add_dependencies(imba_core impala-interface)

target_link_libraries(imba_devices ${PNG_LIBRARIES})
target_link_libraries(imba_devices ${SDL_LIBRARY})
target_link_libraries(imba_loaders ${PNG_LIBRARIES})

# Add executables
add_executable(bench
               ${EMBREE_SRCS}
               bench.cpp
               bench/bench_bvh_build.cpp
               bench/bench_ray_triangle.cpp
               bench/bench_ray_box.cpp
               bench/bench_ray_bvh.cpp)

include_directories(${THORIN_RUNTIME_INCLUDE_DIR})
target_link_libraries(bench ${EMBREE_LIB} ${EMBREE_AVX_LIB} ${EMBREE_AVX2_LIB} ${EMBREE_SSE41_LIB} ${EMBREE_SSE42_LIB} ${EMBREE_SIMD_LIB} ${EMBREE_SYS_LIB})
target_link_libraries(bench ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(bench imba_loaders)
target_link_libraries(bench imba_core)

add_executable(imbatracer
               main.cpp)

target_link_libraries(imbatracer imba_devices)
target_link_libraries(imbatracer imba_loaders)
target_link_libraries(imbatracer imba_core)

# Set compiler options
function(ADD_CXX_FLAG target opt)
    get_target_property(CXX_FLAGS ${target} COMPILE_FLAGS)
    if(CXX_FLAGS STREQUAL "CXX_FLAGS-NOTFOUND")
      set(CXX_FLAGS "")
    endif()
    set(CXX_FLAGS "${CXX_FLAGS} ${opt}")
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${CXX_FLAGS})
endfunction(ADD_CXX_FLAG)

add_cxx_flag(imba_core    "-std=c++11")
add_cxx_flag(imba_loaders "-std=c++11")
add_cxx_flag(imba_devices "-std=c++11")
add_cxx_flag(bench        "-std=c++11")
add_cxx_flag(imbatracer   "-std=c++11")

