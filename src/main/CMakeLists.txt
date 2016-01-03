set(cpp_SRC
    scene.h
    scene.cpp
    objloader.h
    objloader.cpp
    interface.h
    extern_c.cpp
)

set(impala_shared_SRC
    util.impala
    extern_c.impala
    color.impala
    vector.impala
    matrix.impala
    noise.impala
    ray.impala
    bbox.impala
    object.impala
    scene.impala
    state.impala
    texture.impala
    light.impala
    material.impala
    intersection.impala
    view.impala
    cameras.impala
    integrator.impala
    tri.impala
    bvh.impala
    render.impala
)
set(impala_loc_SRC
    mapping_cpu.impala
    interface_loc.impala
)
set(impala_acc_SRC
    mapping_${DEVICE}.impala
    interface_acc.impala
)

THORIN_RUNTIME_WRAP(impala_acc_PROGRAM
    BACKEND ${BACKEND}
    FILES ${impala_shared_SRC} ${impala_acc_SRC})
THORIN_RUNTIME_WRAP(impala_loc_PROGRAM
    BACKEND cpu
    FILES ${impala_shared_SRC} ${impala_loc_SRC})

add_library(rt ${cpp_SRC} ${impala_shared_SRC} ${impala_loc_SRC} ${impala_acc_SRC} ${impala_loc_PROGRAM} ${impala_acc_PROGRAM})
target_link_libraries(rt core ${THORIN_RUNTIME_LIBRARY} ${THORIN_RUNTIME_LIBRARIES})

add_executable(main main.cpp)
target_link_libraries(main rt io ${THORIN_RUNTIME_LIBRARY} ${THORIN_RUNTIME_LIBRARIES})

add_executable(bench bench.cpp)
target_link_libraries(bench rt ${THORIN_RUNTIME_LIBRARY} ${THORIN_RUNTIME_LIBRARIES})

