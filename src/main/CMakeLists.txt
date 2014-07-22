set(cpp_SRC
    scene.h
    scene.cpp
    objloader.h
    objloader.cpp
    interface.h
    extern_c.cpp
)

set(impala_SRC
    util.impala
    extern_c.impala
    color.impala
    vector.impala
    ray.impala
    bbox.impala
    object.impala
    tri.impala
    bvh.impala
    scene.impala
    light.impala
    intersection.impala
    view.impala
    cameras.impala
    integrator.impala
    mapping_${DEVICE}.impala
    interface.impala
)

THORIN_RUNTIME_WRAP(impala_OBJ impala_LIB
    RTTYPE ${RTTYPE}
    FILES ${impala_SRC})

add_library(rt ${cpp_SRC} ${impala_SRC} ${impala_OBJ})
target_link_libraries(rt core)

add_executable(main main.cpp)
target_link_libraries(main rt io)

add_executable(bench bench.cpp)
target_link_libraries(bench rt)

