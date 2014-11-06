set(cpp_SRC
    objloader.h
    objloader.cpp
    interface.h
    extern_c.cpp
)

set(impala_shared_SRC
    util.impala
    extern_c.impala
    dynarray.impala
    color.impala
    vector.impala
    matrix.impala
    noise.impala
    image.impala
    ray.impala
    bbox.impala
    instance.impala
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
    bvh_construction.impala
)
set(impala_loc_SRC
    assert_cpu.impala
    tri_build.impala
    camcontrol.impala
    imageload.impala
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

set(rt_SRC ${cpp_SRC} ${impala_shared_SRC} ${impala_loc_SRC} ${impala_acc_SRC} ${impala_loc_OBJ} ${impala_acc_OBJ})

# we could put rt_SRC into a library and link that into main and bench, but then we'd loose "undefined symbol" warnings...

add_executable(main main.cpp ${rt_SRC})
target_link_libraries(main io ${impala_loc_LIB} ${impala_acc_LIB})

add_executable(bench bench.cpp ${rt_SRC})
target_link_libraries(bench io ${impala_loc_LIB} ${impala_acc_LIB})

