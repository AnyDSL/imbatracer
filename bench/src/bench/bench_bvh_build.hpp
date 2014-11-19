#ifndef BENCH_BVH_BUILD_HPP
#define BENCH_BVH_BUILD_HPP

#include "../impala/impala_interface.h"
#include "../common/memory.hpp"
#include "../scene/triangle_mesh.hpp"
#include "bench.hpp"

namespace embree {
    class Scene;
}

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
    virtual void iteration() override;
    virtual void display() override;

private:
    imba::ThorinUniquePtr<BenchBvhBuildResult> result_;
    const imba::TriangleMesh* mesh_;
    int nodes_;
};

class BenchBvh4BuildEmbree : public Bench {
public:
    BenchBvh4BuildEmbree(const imba::TriangleMesh* mesh);

protected:
    virtual void iteration() override;

private:
    std::unique_ptr<embree::Scene> scene_;
    unsigned int mesh_id_;
};

} // namespace bench

#endif // BENCH_BVH_BUILD_HPP

