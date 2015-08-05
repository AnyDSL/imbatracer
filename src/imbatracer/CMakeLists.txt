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
            core/bvh_builder.cpp
            core/bvh_builder.h
            core/split.cpp
            core/split.h
            core/tri.cpp
            core/tri.h)

add_library(imba-render
            render/image.h
            render/camera.h
            render/camera.cpp
            render/render.h
            render/render.cpp
            render/shader.h
            render/shader.cpp)
            
find_package(CUDA REQUIRED)
target_link_libraries(imba-render ${TRAVERSAL_LIBRARY} ${CUDA_LIBRARIES})
target_link_libraries(imba-core ${TRAVERSAL_LIBRARY} ${CUDA_LIBRARIES})
            
add_executable(imbatracer
               frontend/main.cpp
               frontend/SDL_device.h
               frontend/SDL_device.cpp
               frontend/test_scene.h)
target_link_libraries(imbatracer ${SDL_LIBRARY} imba-render imba-core ${CUDA_LIBRARIES})
