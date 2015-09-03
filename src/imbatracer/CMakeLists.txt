add_library(imba-core
            core/adapter.h
            core/allocator.h
            core/bbox.h
            core/common.h
            core/float2.h
            core/float3.h
            core/float4.h
            core/gpu_adapter.cpp
            core/mem_pool.h
            core/mesh.h
            core/sbvh_builder.h
            core/tri.h)

add_library(imba-render
            render/image.h
            render/camera.h
            render/camera.cpp
            render/render.h
            render/render.cpp
            render/shader.h
            render/shader.cpp
            render/ray_queue.h
            render/random.h)
            
target_link_libraries(imba-render ${TRAVERSAL_LIBRARIES})
target_link_libraries(imba-core ${TRAVERSAL_LIBRARIES})

add_executable(imbatracer
               frontend/main.cpp
               frontend/SDL_device.h
               frontend/SDL_device.cpp
               frontend/test_scene.h
               frontend/obj_loader.h
               frontend/obj_loader.cpp
               frontend/path.h
               frontend/logger.h)
target_link_libraries(imbatracer ${SDL_LIBRARY} imba-render imba-core ${TRAVERSAL_LIBRARIES})
