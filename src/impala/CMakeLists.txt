# Generate object file from impala sources
set(IMPALA_SRCS
    intersection/ray_triangle.impala
    intersection/ray_box.impala
    intersection/packet_box.impala
    intersection/packet_tri.impala
    intersection/frustum_box.impala
    math/ray.impala
    math/box.impala
    math/matrix.impala
    math/vector.impala
    math/triangle.impala
    math/limits.impala
    math/packet.impala
    math/frustum.impala
    math/random.impala
    accel/bvh.impala
    common/range.impala
    common/debug.impala
    common/vectorize.impala
    scene/image.impala
    scene/scene.impala
    scene/mesh.impala
    scene/material.impala
    scene/light.impala
    scene/render.impala)

set(IMPALA_BENCH_SRCS
    bench/bench_bvh_build.impala
    bench/bench_ray_box.impala
    bench/bench_ray_bvh.impala
    bench/bench_ray_triangle.impala)

set(IMPALA_INTERFACE_SRCS
    interface.impala)

