#define _X86INTRIN_H_INCLUDED
#include <geometry/triangle1_intersector1_moeller.h>
#include <geometry/triangle1_intersector4_moeller.h>
#include "bench_ray_triangle.hpp"

#define COUNT 4000000

struct bench_ray_tri_result
{
    int intr_count;
    float tmin;
};

extern "C" bench_ray_tri_result* bench_ray_triangle(int, const float*, const float*, const float*);
extern "C" bench_ray_tri_result* bench_ray4_triangle(int, const float*, const float*, const float*);

namespace bench {

void BenchRayTriangleImpala::iteration()
{
    const float v0[3] = {0.2f, 0.0f, 0.0f};
    const float v1[3] = {0.2f, 1.0f, 0.0f};
    const float v2[3] = {0.2f, 0.0f, 1.0f};

    bench_ray_tri_result* result = bench_ray_triangle(nrays_, v0, v1, v2);

    icount_ = result->intr_count;
    tmin_ = result->tmin;
    
    printf("%d %f\n", icount_, tmin_);

    free(result);
}

void BenchRay4TriangleImpala::iteration()
{
    const float v0[3] = {0.2f, 0.0f, 0.0f};
    const float v1[3] = {0.2f, 1.0f, 0.0f};
    const float v2[3] = {0.2f, 0.0f, 1.0f};

    bench_ray_tri_result* result = bench_ray4_triangle(nrays_, v0, v1, v2);

    icount_ = result->intr_count;
    tmin_ = result->tmin;

    printf("%d %f\n", icount_, tmin_);

    free(result);
}

void BenchRayTriangleEmbree::iteration()
{
    embree::Triangle1 tri(embree::Vec3fa(0.2f, 0.0f, 0.0f),
                          embree::Vec3fa(0.2f, 0.0f, 1.0f),
                          embree::Vec3fa(0.2f, 1.0f, 0.0f),
                          0, 0, 0xFFFFFFFF, 0);

    icount_ = 0;
    tmin_ = embree::inf;

    for (int i = 0; i < nrays_; i++) {
        int j = i % 1000;
        embree::Ray ray(embree::Vec3fa(-1.0f, 0.001f + j * 0.001f, 0.001f + j * 0.001f),
                        embree::Vec3fa(1.0f, 0.0f, 0.0f));

        embree::Triangle1Intersector1MoellerTrumbore<false>::Precalculations precalc(ray);
        embree::Triangle1Intersector1MoellerTrumbore<false>::intersect(precalc, ray, tri, nullptr);

        tmin_ = (tmin_ < ray.tfar) ? tmin_ : ray.tfar;
        icount_ += ray.primID + 1;
    }

    printf("%d %f\n", icount_, tmin_);
}


void BenchRay4TriangleEmbree::iteration()
{
    embree::Triangle1 tri(embree::Vec3fa(0.2f, 0.0f, 0.0f),
                          embree::Vec3fa(0.2f, 0.0f, 1.0f),
                          embree::Vec3fa(0.2f, 1.0f, 0.0f),
                          0, 0, 0xFFFFFFFF, 0);

    icount_ = 0;
    tmin_ = embree::inf;

    for (int i = 0; i < nray4s_; i++) {
        embree::ssef inc(0.001f + ( i      % 1000) * 0.001f,
                         0.001f + ((i + 1) % 1000) * 0.001f,
                         0.001f + ((i + 2) % 1000) * 0.001f,
                         0.001f + ((i + 3) % 1000) * 0.001f);

        embree::Ray4 ray(embree::sse3f(embree::ssef(-1.0f, -1.0f, -1.0f, -1.0f),
                                       inc, inc),
                         embree::sse3f(embree::ssef(1.0f, 1.0f, 1.0f, 1.0f),
                                       embree::ssef(0.0f, 0.0f, 0.0f, 0.0f),
                                       embree::ssef(0.0f, 0.0f, 0.0f, 0.0f)));
        embree::sseb valid(true, true, true, true);

        embree::Triangle1Intersector4MoellerTrumbore<false>::Precalculations precalc(valid, ray);
        embree::Triangle1Intersector4MoellerTrumbore<false>::intersect(valid, precalc, ray, tri, nullptr);

        tmin_ = (tmin_ < ray.tfar.f[0]) ? tmin_ : ray.tfar.f[0];
        tmin_ = (tmin_ < ray.tfar.f[1]) ? tmin_ : ray.tfar.f[1];
        tmin_ = (tmin_ < ray.tfar.f[2]) ? tmin_ : ray.tfar.f[2];
        tmin_ = (tmin_ < ray.tfar.f[3]) ? tmin_ : ray.tfar.f[3];

        icount_ += ray.primID.i[0] + 1;
        icount_ += ray.primID.i[1] + 1;
        icount_ += ray.primID.i[2] + 1;
        icount_ += ray.primID.i[3] + 1;
    }

    printf("%d %f\n", icount_, tmin_);
}

} // namespace bench

