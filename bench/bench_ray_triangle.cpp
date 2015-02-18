#include <iostream>
#include "bench_ray_triangle.hpp"

#include "geometry/triangle1_intersector1_moeller.h"
#include "geometry/triangle1_intersector4_moeller.h"

namespace bench {

void BenchRayTriangleImpala::iteration() {
    float v0[3] = {0.2f, 0.0f, 0.0f};
    float v1[3] = {0.2f, 1.0f, 0.0f};
    float v2[3] = {0.2f, 0.0f, 1.0f};

    bench_ray_triangle(nrays_, v0, v1, v2, result_.get());

    icount_ = result_->intr_count;
    tmin_ = result_->tmin;
}

void BenchRayTriangleImpala::display() {
    std::cout << icount_ << " " << tmin_ << std::endl;
}

void BenchRay4TriangleImpala::iteration() {
    float v0[3] = {0.2f, 0.0f, 0.0f};
    float v1[3] = {0.2f, 1.0f, 0.0f};
    float v2[3] = {0.2f, 0.0f, 1.0f};

    bench_ray4_triangle(nrays_, v0, v1, v2, result_.get());

    icount_ = result_->intr_count;
    tmin_ = result_->tmin;
}

void BenchRay4TriangleImpala::display() {
    std::cout << icount_ << " " << tmin_ << std::endl;
}

void BenchRayTriangleEmbree::iteration() {
    using namespace embree;
    using namespace embree::isa;
    Triangle1 tri(Vec3fa(0.2f, 0.0f, 0.0f),
                  Vec3fa(0.2f, 0.0f, 1.0f),
                  Vec3fa(0.2f, 1.0f, 0.0f),
                  0, 0, 0xFFFFFFFF, 0);

    icount_ = 0;
    tmin_ = embree::inf;

    for (int i = 0; i < nrays_; i++) {
        int j = i % 1000;
        Ray ray(Vec3fa(-1.0f, 0.001f + j * 0.001f, 0.001f + j * 0.001f),
                Vec3fa(1.0f, 0.0f, 0.0f));

        Triangle1Intersector1MoellerTrumbore<false>::Precalculations precalc(ray);
        Triangle1Intersector1MoellerTrumbore<false>::intersect(precalc, ray, tri, nullptr);

        tmin_ = (tmin_ < ray.tfar) ? tmin_ : ray.tfar;
        icount_ += ray.primID + 1;
    }
}

void BenchRayTriangleEmbree::display() {
    std::cout << icount_ << " " << tmin_ << std::endl;
}

void BenchRay4TriangleEmbree::iteration() {
    using namespace embree;
    using namespace embree::isa;
    Triangle1 tri(Vec3fa(0.2f, 0.0f, 0.0f),
                          Vec3fa(0.2f, 0.0f, 1.0f),
                          Vec3fa(0.2f, 1.0f, 0.0f),
                          0, 0, 0xFFFFFFFF, 0);

    icount_ = 0;
    tmin_ = embree::inf;

    for (int i = 0; i < nray4s_; i++) {
        ssef inc(0.001f + ( i      % 1000) * 0.001f,
                         0.001f + ((i + 1) % 1000) * 0.001f,
                         0.001f + ((i + 2) % 1000) * 0.001f,
                         0.001f + ((i + 3) % 1000) * 0.001f);

        Ray4 ray(sse3f(ssef(-1.0f, -1.0f, -1.0f, -1.0f), inc, inc),
                 sse3f(ssef(1.0f, 1.0f, 1.0f, 1.0f),
                       ssef(0.0f, 0.0f, 0.0f, 0.0f),
                       ssef(0.0f, 0.0f, 0.0f, 0.0f)));
        sseb valid(true, true, true, true);

        Triangle1Intersector4MoellerTrumbore<false>::Precalculations precalc(valid, ray);
        Triangle1Intersector4MoellerTrumbore<false>::intersect(valid, precalc, ray, tri, nullptr);

        tmin_ = (tmin_ < ray.tfar.f[0]) ? tmin_ : ray.tfar.f[0];
        tmin_ = (tmin_ < ray.tfar.f[1]) ? tmin_ : ray.tfar.f[1];
        tmin_ = (tmin_ < ray.tfar.f[2]) ? tmin_ : ray.tfar.f[2];
        tmin_ = (tmin_ < ray.tfar.f[3]) ? tmin_ : ray.tfar.f[3];

        icount_ += ray.primID.i[0] + 1;
        icount_ += ray.primID.i[1] + 1;
        icount_ += ray.primID.i[2] + 1;
        icount_ += ray.primID.i[3] + 1;
    }
}

void BenchRay4TriangleEmbree::display() {
    std::cout << icount_ << " " << tmin_ << std::endl;
}

} // namespace bench

