#include <iostream>
#include "bench_ray_bvh.hpp"

namespace bench {

void BenchRayBvhImpala::iteration() {
    bench_ray_bvh(nrays_, &build_->bvh, (Vec3*)mesh_->vertices(), (int*)mesh_->triangles(), result_.get());
    tmin_ = result_->tmin;
    tmax_ = result_->tmax;
    icount_ = result_->intr_count;
}

void BenchRayBvhImpala::display() {
    std::cout << tmin_ << " " << tmax_ << " " << icount_ << std::endl;
}

void BenchRay4BvhImpala::iteration() {
    bench_ray4_bvh(nrays_, &build_->bvh, (Vec3*)mesh_->vertices(), (int*)mesh_->triangles(), result_.get());
    tmin_ = result_->tmin;
    tmax_ = result_->tmax;
    icount_ = result_->intr_count;
}

void BenchRay4BvhImpala::display() {
    std::cout << tmin_ << " " << tmax_ << " " << icount_ << std::endl;
}

void BenchRayBvh4Embree::iteration() {
    
}

void BenchRayBvh4Embree::display() {
    
}

void BenchRay4Bvh4Embree::iteration() {
    
}

void BenchRay4Bvh4Embree::display() {
    
}

} // namespace bench

