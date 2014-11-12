#include <iostream>
#include "bench_ray_box.hpp"

namespace bench {

void BenchRayBoxImpala::iteration() {
    float min[3] = {0.2f, 0.0f, 0.0f};
    float max[3] = {1.2f, 1.0f, 1.0f};

    bench_ray_box(nrays_, min, max, result_.get());

    icount_ = result_->intr_count;
    tmin_ = result_->tmin;
    tmax_ = result_->tmax;
}

void BenchRayBoxImpala::display() {
    std::cout << icount_ << " " << tmin_ << " " << tmax_ << std::endl;
}

void BenchRay4BoxImpala::iteration() {
    float min[3] = {0.2f, 0.0f, 0.0f};
    float max[3] = {1.2f, 1.0f, 1.0f};

    bench_ray4_box(nrays_, min, max, result_.get());

    icount_ = result_->intr_count;
    tmin_ = result_->tmin;
    tmax_ = result_->tmax;
}

void BenchRay4BoxImpala::display() {
    std::cout << icount_ << " " << tmin_ << " " << tmax_ << std::endl;
}

} // namespace bench

