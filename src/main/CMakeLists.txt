set(main_cpp_SRC
scene.cpp
extern_c.cpp
main.cpp
)

set(main_impala_SRC
extern_c.impala
util.impala
color.impala
vector.impala
ray.impala
scene.impala
tri.impala
intersection.impala
view.impala
cameras.impala
integrator.impala
render.impala
test.impala # compiler testing only
interface.impala
)

THORIN_RUNTIME_WRAP(main_impala_OBJ main_impala_LIB
    RTTYPE cpu
    FILES ${main_impala_SRC})

set(main_SRC ${main_cpp_SRC} ${main_impala_OBJ} ${main_impala_SRC})

add_executable(main ${main_SRC})
target_link_libraries(main io ${main_impala_LIB})

