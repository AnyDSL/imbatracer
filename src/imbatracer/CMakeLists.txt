# Configure traversal platform
if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
  add_definitions(-DCPU_TRAVERSAL)
  list(APPEND CORE_SRCS core/cpu_adapter.cpp)
  #set(LOADER_SRCS loaders/load_mbvh.cpp)
elseif(BACKEND STREQUAL "nvvm" OR BACKEND STREQUAL "cuda" OR BACKEND STREQUAL "gpu")
  add_definitions(-DGPU_TRAVERSAL)
  list(APPEND CORE_SRCS core/gpu_adapter.cpp)
  #set(LOADER_SRCS loaders/load_bvh.cpp)
else()
  message(FATAL_ERROR "Backend not supported")
endif()

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
            core/bvh_helper.h
            core/sbvh_builder.h
            core/fast_bvh_builder.h
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
            render/scene.cpp
            render/texture_sampler.h
            render/ray_scheduler.h
            rangesearch/rangesearch.h)

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
                      ${SDL2_LIBRARY}
                      ${PNG_LIBRARIES}
                      ${TRAVERSAL_LIBRARY}
                      ${THORIN_RUNTIME_LIBRARY}
                      ${THORIN_RUNTIME_LIBRARIES}
                      ${TBB_LIBRARIES})

