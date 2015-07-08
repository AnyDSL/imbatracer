set(RENDER_SRCS
    basic_shader.impala
    interface.impala
    light.impala
    range.impala
    ray.impala
    render.impala
    scene.impala
    shader.impala
    triangle.impala
    vector.impala
    buffers.impala)

set(IMPALA_SRCS ${RENDER_SRCS})

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/frontend/interface.h
                   COMMAND impala -emit-c-interface ${IMPALA_SRCS}
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o interface
                   DEPENDS ${IMPALA_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS})

set_source_files_properties(interface.h PROPERTIES GENERATED TRUE)

add_library(render ${IMPALA_OBJS})
target_link_libraries(render ${IMPALA_LIBS})

