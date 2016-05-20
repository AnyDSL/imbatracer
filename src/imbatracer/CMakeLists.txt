# Configure traversal platform
if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
  add_definitions(-DCPU_TRAVERSAL)
  set(CORE_SRCS core/cpu_adapter.cpp)
  set(LOADER_SRCS loaders/load_mbvh.cpp)
elseif(BACKEND STREQUAL "nvvm" OR BACKEND STREQUAL "cuda" OR BACKEND STREQUAL "gpu")
  add_definitions(-DGPU_TRAVERSAL)
  set(CORE_SRCS core/gpu_adapter.cpp)
  set(LOADER_SRCS loaders/load_bvh.cpp)
else()
  message(FATAL_ERROR "Backend not supported")
endif()

set(IMPALA_SRCS
    ~/Documents/AnyDSL/traversal/src/common/range.impala
    rangesearch/rangesearch.impala
    rangesearch/mapping_gpu.impala)
set(IMPALA_INTERFACE_NAME rangesearch_impala_interface)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/rangesearch/${IMPALA_INTERFACE_NAME}.h
                   COMMAND impala -emit-c-interface ${IMPALA_SRCS}
                          ${THORIN_RUNTIME_DIR}/runtime.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o rangesearch/${IMPALA_INTERFACE_NAME}
                   DEPENDS ${IMPALA_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(IMPALA_PROGRAM
                    BACKEND ${BACKEND}
                    CLANG_FLAGS -march=native -ffast-math
                    FILES ${IMPALA_SRCS})

set_source_files_properties(rangesearch/${IMPALA_INTERFACE_NAME}.h PROPERTIES GENERATED TRUE)

set(IMPALA_INTERFACES # Have to use it somewhere so the interface head files will be generated
    rangesearch/${IMPALA_INTERFACE_NAME}.h)

add_library(imba_core
            core/adapter.h
            core/bbox.h
            core/common.h
            core/float2.h
            core/float3.h
            core/float4.h
            core/float4x4.h
            core/mask.h
            core/mem_pool.h
            core/mesh.h
            core/mesh.cpp
            core/sbvh_builder.h
            core/tri.h
            ${CORE_SRCS})

add_library(imba_loaders
            loaders/loaders.h
            loaders/load_png.cpp
            loaders/load_tga.cpp
            loaders/load_obj.h
            loaders/load_obj.cpp
            loaders/path.h
            loaders/bvh_format.h
            ${LOADER_SRCS})

add_library(imba_render
            render/traversal_mutex.cpp
            render/camera.h
            render/integrators/integrator.h
            render/integrators/pt.h
            render/integrators/pt.cpp
            render/integrators/vcm.h
            render/integrators/vcm.cpp
            render/ray_queue.h
            render/random.h
            render/intersection.h
            render/mem_arena.h
            render/scene.h
            render/thorin_mem.h
            render/texture_sampler.h
            render/ray_scheduler.h
            rangesearch/rangesearch.h
            ${IMPALA_INTERFACES})

# Rangesearch Impala library
add_library(rangesearch ${IMPALA_PROGRAM})
target_link_libraries(rangesearch ${THORIN_RUNTIME_LIBRARY} ${THORIN_RUNTIME_LIBRARIES})

add_executable(imbatracer
               frontend/main.cpp
               frontend/render_window.h
               frontend/render_window.cpp
               frontend/build_scene.h
               frontend/build_scene.cpp)

target_link_libraries(imbatracer
                      imba_render
                      imba_core
                      imba_loaders
                      rangesearch
                      ${SDL2_LIBRARY}
                      ${PNG_LIBRARIES}
                      ${TRAVERSAL_LIBRARY}
                      ${THORIN_RUNTIME_LIBRARY}
                      ${THORIN_RUNTIME_LIBRARIES}
                      ${TBB_LIBRARIES})

