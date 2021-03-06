add_library(imba_core
            core/adapter.h
            core/atomic_vector.h
            core/bbox.h
            core/bsphere.h
            core/bvh_helper.h
            core/common.h
            core/rgb.h
            core/float4x4.h
            core/float3x4.h
            core/float2.h
            core/float3.h
            core/float4.h
            core/image.h
            core/mask.h
            core/mem_pool.h
            core/mesh.h
            core/mesh.cpp
            core/sbvh_builder.h
            core/fast_bvh_builder.h
            core/stack.h
            core/tri.h
            core/cpu_adapter.cpp
            core/gpu_adapter.cpp)

add_library(imba_loaders
            loaders/loaders.h
            loaders/load_png.cpp
            loaders/load_tga.cpp
            loaders/load_obj.h
            loaders/load_obj.cpp
            loaders/path.h
            loaders/load_bvh.cpp
            loaders/load_hdr.cpp)

add_library(imba_render
            render/light.h
            render/random.h
            render/intersection.h
            render/mem_arena.h
            render/scene.h
            render/scene.cpp
            render/texture_sampler.h

            render/integrators/integrator.h
            render/integrators/integrator.cpp
            render/integrators/pt.h
            render/integrators/pt.cpp
            render/integrators/vcm.h
            render/integrators/vcm.cpp
            render/integrators/light_vertices.h
            render/integrators/light_vertices.cpp

            render/debug/path_debug.h
            render/debug/mis_debug.h

            render/scheduling/ray_queue.h
            render/scheduling/queue_scheduler.h
            render/scheduling/tile_scheduler.h
            render/scheduling/ray_scheduler.h

            render/ray_gen/ray_gen.h
            render/ray_gen/tile_gen.h
            render/ray_gen/camera.h

            core/traversal_interface.h
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
                      ${TRAVERSAL_LIBRARY_CPU}
                      ${TRAVERSAL_LIBRARY_GPU}
                      ${AnyDSL_runtime_LIBRARIES}
                      ${TBB_LIBRARIES})

