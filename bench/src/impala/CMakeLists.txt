# Generate object file from impala sources
set(IMPALA_SRCS
    bench/bench_ray_triangle.impala
    bench/bench_ray_box.impala
    bench/bench_bvh_build.impala
    bench/bench_ray_bvh.impala
    intersection/ray_triangle.impala
    intersection/ray_box.impala
    math/ray.impala
    math/box.impala
    math/matrix.impala
    math/vector.impala
    math/triangle.impala
    math/limits.impala
    accel/bvh.impala
    common/range.impala
    common/debug.impala
    scene/image.impala
    scene/scene.impala
    scene/mesh.impala
    scene/render.impala)

