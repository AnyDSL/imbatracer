add_library(imba_core
            core/adapter.h
            core/allocator.h
            core/bbox.h
            core/common.h
            core/float2.h
            core/float3.h
            core/float4.h
            core/gpu_adapter.cpp
            #core/cpu_adapter.cpp
            core/mem_pool.h
            core/mesh.h
            core/mesh.cpp
            core/sbvh_builder.h
            core/tri.h)

add_library(imba_loaders
            loaders/loaders.h
            loaders/load_png.cpp
            loaders/load_tga.cpp
            loaders/load_obj.h
            loaders/load_obj.cpp
            loaders/load_zmod.h
            loaders/load_zmod.cpp
            loaders/path.h)

add_library(imba_render
            render/camera.h
            render/render.h
            render/integrators/integrator.h
            render/integrators/pt.cpp
            #render/integrators/bpt.cpp
            render/integrators/pt.h
            render/integrators/bpt.h
            #render/integrators/vcm.cpp
            render/ray_queue.h
            render/random.h
            render/material.h
            render/scene.h
            render/thorin_mem.h
            render/texture_sampler.h)

add_executable(imbatracer
               frontend/main.cpp
               frontend/render_window.h
               frontend/render_window.cpp
               frontend/test_scene.h
               frontend/build_scene.h
               frontend/build_scene.cpp)

target_link_libraries(imbatracer
                      imba_render
                      imba_core
                      imba_loaders
                      ${SDL_LIBRARY}
                      ${PNG_LIBRARIES}
                      ${ZLIB_LIBRARIES}
                      ${TRAVERSAL_LIBRARY}
                      ${THORIN_RUNTIME_LIBRARY}
                      ${THORIN_RUNTIME_LIBRARIES})

add_executable(zmodel tools/zmodel.cpp)

target_link_libraries(zmodel imba_core imba_loaders ${ZLIB_LIBRARIES})
