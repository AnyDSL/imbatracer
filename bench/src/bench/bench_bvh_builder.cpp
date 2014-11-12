#include "bench_bvh_builder.hpp"
#include <iostream>

namespace bench {

void BenchBvhBuildImpala::iteration()
{
    bench_bvh_build((Vec3*)mesh_->vertices(),
                    (int*)mesh_->triangles(),
                    mesh_->triangle_count(),
                    result_.get());

    nodes_ = result_->bvh.node_count;

    thorin_free(result_->bvh.nodes);
    thorin_free(result_->bvh.prim_ids);
    thorin_free(result_->boxes);
    thorin_free(result_->centers);
}

void BenchBvhBuildImpala::display() {
    std::cout << nodes_ << std::endl;
}

} // namespace bench

