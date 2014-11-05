# Generate object file from impala sources
set(IMPALA_SOURCES
    bench/bench_ray_triangle.impala
    bench/bench_ray_box.impala
    bench/bench_ray_bvh.impala
    intersection/ray_triangle.impala
    intersection/ray_box.impala
    intersection/ray_bvh.impala
    math/ray.impala
    math/box.impala
    math/matrix.impala
    math/vector.impala
    math/triangle.impala
    math/limits.impala
    accel/bvh.impala
    common/range.impala
    common/debug.impala)

find_package(THORIN REQUIRED)

add_custom_command(OUTPUT impala_interface.ll
                   DEPENDS ${IMPALA_SOURCES}
                   # Output of impala is always in the source folder ? Must be a bug.
                   #COMMAND impala -O3 -emit-llvm ${IMPALA_SOURCES} -o "${CMAKE_CURRENT_BINARY_DIR}/impala_interface.ll"
                   COMMAND impala -O3 -emit-llvm ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala ${THORIN_RUNTIME_DIR}/platforms/intrinsics_cpu.impala ${IMPALA_SOURCES}
                   COMMAND opt -O3 debug.ll -S -o "${CMAKE_CURRENT_BINARY_DIR}/impala_interface.ll"
                   COMMAND clang -O3 -march=native -S -c -fslp-vectorize-aggressive "${CMAKE_CURRENT_BINARY_DIR}/impala_interface.ll" -o "${CMAKE_CURRENT_BINARY_DIR}/impala_interface.s"
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(OUTPUT impala_interface.o
                   DEPENDS impala_interface.ll
                   COMMAND clang -O3 -march=native impala_interface.ll -c -o impala_interface.o)

add_custom_target(impala_interface DEPENDS impala_interface.o)

