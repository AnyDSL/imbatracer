# Generate object file from impala sources
set(IMPALA_SOURCES
    #bench_ray_triangle.impala
    bench_ray4_triangle.impala
    ray_triangle2.impala
    )#vector.impala)

find_package(THORIN REQUIRED)

add_custom_command(OUTPUT impala_interface.ll
                   DEPENDS ${IMPALA_SOURCES}
                   # Output of impala is always in the source folder ? Must be a bug.
                   #COMMAND impala -O3 -emit-llvm ${IMPALA_SOURCES} -o "${CMAKE_CURRENT_BINARY_DIR}/impala_interface.ll"
                   COMMAND impala -O3 -emit-llvm ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala ${IMPALA_SOURCES}
                   COMMAND opt -O3 ray_triangle2.ll -S -o "${CMAKE_CURRENT_BINARY_DIR}/impala_interface.ll"
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(OUTPUT impala_interface.o
                   DEPENDS impala_interface.ll
                   COMMAND clang -O3 -march=native impala_interface.ll -c -o impala_interface.o)

add_custom_target(impala_interface DEPENDS impala_interface.o)

