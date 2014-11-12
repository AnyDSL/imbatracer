#ifndef BENCH_BVH_BUILDER_HPP
#define BENCH_BVH_BUILDER_HPP

#include "../impala/impala_interface.h"
#include "../common/memory.hpp"
#include "bench.hpp"

namespace bench {

class BenchBvhBuildImpala : public Bench {
public:
    BenchBvhBuildImpala()
        : Bench("bench_bvh_builder_impala")
        , result_(imba::thorin_make_unique<BenchBvhBuildResult>())
    {}

    int get_node_count() { return nodes_; }

protected:
    virtual void iteration();

private:
    imba::ThorinUniquePtr<BenchBvhBuildResult> result_;
    int nodes_;
};

} // namespace bench

#endif

