#ifndef BENCH_BVH_BUILDER_HPP
#define BENCH_BVH_BUILDER_HPP

#include "../impala/impala_interface.h"
#include "../common/memory.hpp"
#include "../scene/triangle_mesh.hpp"
#include "bench.hpp"

namespace bench {

class BenchBvhBuildImpala : public Bench {
public:
    BenchBvhBuildImpala(const imba::TriangleMesh* mesh)
        : Bench("bench_bvh_builder_impala")
        , result_(imba::thorin_make_unique<BenchBvhBuildResult>())
        , mesh_(mesh)
    {}

    int node_count() { return nodes_; }

protected:
    virtual void iteration();
    virtual void display();

private:
    imba::ThorinUniquePtr<BenchBvhBuildResult> result_;
    const imba::TriangleMesh* mesh_;
    int nodes_;
};

} // namespace bench

#endif // BENCH_BVH_BUILDER_HPP

