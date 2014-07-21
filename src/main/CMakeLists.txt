set(main_cpp_SRC
objloader.h
objloader.cpp
scene.h
scene.cpp
interface.h
extern_c.cpp
main.cpp
)

set(main_impala_SRC
mapping_${DEVICE}.impala
extern_c.impala
util.impala
color.impala
vector.impala
ray.impala
scene.impala
bbox.impala
tri.impala
object.impala
light.impala
intersection.impala
view.impala
cameras.impala
integrator.impala
render.impala
test.impala # compiler testing only
interface.impala
)

THORIN_RUNTIME_WRAP(main_impala_OBJ main_impala_LIB
    RTTYPE ${RTTYPE}
    FILES ${main_impala_SRC})

set(main_SRC ${main_cpp_SRC} ${main_impala_OBJ} ${main_impala_SRC})

add_executable(main ${main_SRC})
target_link_libraries(main io ${main_impala_LIB})

