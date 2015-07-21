set(FRONTEND_SRCS
    frontend/main.cpp
    frontend/SDL_device.h
    frontend/SDL_device.cpp
    frontend/render.h
    frontend/test_scene.h)

set(RENDER_SRCS
    render/shader/basic_shader.impala
    render/common/traversal_interface.impala
    render/math/float.impala
    render/scene/light.impala
    render/common/range.impala
    render/common/debug.impala
    render/rays/ray.impala
    render/rays/camera.impala
    render/render.impala
    render/scene/scene.impala
    render/shader/shader.impala
    render/math/triangle.impala
    render/math/vector.impala
    render/math/random.impala)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/frontend/render.h
                   COMMAND impala -emit-c-interface ${RENDER_SRCS}
                           ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                           ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                           -o frontend/render
                   DEPENDS ${RENDER_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(RENDER_OBJS RENDER_LIBS
                    BACKEND ${BACKEND}
                    FILES ${RENDER_SRCS})

set_source_files_properties(frontend/render.h PROPERTIES GENERATED TRUE)

add_library(render ${RENDER_OBJS})
target_link_libraries(render ${RENDER_LIBS})

add_executable(frontend ${FRONTEND_SRCS})
target_link_libraries(frontend render ${TRAVERSAL_LIBRARY} ${SDL_LIBRARY})
