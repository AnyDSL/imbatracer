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
    bvh_traversal.impala
)
set(impala_loc_SRC
    assert_cpu.impala
    bvh_construction.impala
    interface_loc.impala
)
set(impala_acc_SRC
    render.impala
    assert_${DEVICE}.impala
    mapping_${DEVICE}.impala
    interface_acc.impala
)

THORIN_RUNTIME_WRAP(impala_acc_OBJ impala_acc_LIB
    RTTYPE ${RTTYPE}
    FILES ${impala_shared_SRC} ${impala_acc_SRC})
THORIN_RUNTIME_WRAP(impala_loc_OBJ impala_loc_LIB
    RTTYPE none
    FILES ${impala_shared_SRC} ${impala_loc_SRC})

add_library(rt ${cpp_SRC} ${impala_shared_SRC} ${impala_loc_SRC} ${impala_acc_SRC} ${impala_loc_OBJ} ${impala_acc_OBJ})
target_link_libraries(rt core ${impala_loc_LIB} ${impala_acc_LIB})

add_executable(main main.cpp)
target_link_libraries(main rt io)

add_executable(bench bench.cpp)
target_link_libraries(bench rt)

