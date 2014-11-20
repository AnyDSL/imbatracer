#ifndef BENCH_RAY_BVH_HPP
#define BENCH_RAY_BVH_HPP

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

#include "../impala/impala_interface.h"
#include "../scene/triangle_mesh.hpp"
#include "../common/memory.hpp"
#include "bench.hpp"

namespace bench {

class BenchRayBvhImpala : public Bench {
public:
    BenchRayBvhImpala(const imba::TriangleMesh* mesh, int nrays)
        : Bench("bench_ray_bvh_impala")
        , build_(imba::thorin_make_unique<BenchBvhBuildResult>())
        , result_(imba::thorin_make_unique<BenchRayBvhResult>())
        , mesh_(mesh)
        , nrays_(nrays)
    {
        bench_bvh_build((Vec3*)mesh->vertices(),
                        (int*)mesh->triangles(),
                        mesh->triangle_count(),
                        build_.get());

        thorin_free(build_->boxes);
        thorin_free(build_->centers);
    }

    ~BenchRayBvhImpala() {
        thorin_free(build_->bvh.nodes);
        thorin_free(build_->bvh.prim_ids);
    }

    float tmin() const { return tmin_; }
    float tmax() const { return tmax_; }
    int intr_count() const { return icount_; }

protected:
    virtual void iteration() override;
    virtual void display() override;

private:
    imba::ThorinUniquePtr<BenchBvhBuildResult> build_;
    imba::ThorinUniquePtr<BenchRayBvhResult> result_;
    const imba::TriangleMesh* mesh_;
    int nrays_, icount_;
    float tmin_, tmax_;
};

class BenchRay4BvhImpala : public Bench {
public:
    BenchRay4BvhImpala(const imba::TriangleMesh* mesh, int nray4s)
        : Bench("bench_ray4_bvh_impala")
        , build_(imba::thorin_make_unique<BenchBvhBuildResult>())
        , result_(imba::thorin_make_unique<BenchRayBvhResult>())
        , mesh_(mesh)
        , nrays_(nray4s)
    {
        bench_bvh_build((Vec3*)mesh->vertices(),
                        (int*)mesh->triangles(),
                        mesh->triangle_count(),
                        build_.get());

        thorin_free(build_->boxes);
        thorin_free(build_->centers);
    }

    ~BenchRay4BvhImpala() {
        thorin_free(build_->bvh.nodes);
        thorin_free(build_->bvh.prim_ids);
    }

    float tmin() const { return tmin_; }
    float tmax() const { return tmax_; }
    int intr_count() const { return icount_; }

protected:
    virtual void iteration() override;
    virtual void display() override;

private:
    imba::ThorinUniquePtr<BenchBvhBuildResult> build_;
    imba::ThorinUniquePtr<BenchRayBvhResult> result_;
    const imba::TriangleMesh* mesh_;
    int nrays_, icount_;
    float tmin_, tmax_;
};

class BenchRayBvh4Embree : public Bench {
public:
    BenchRayBvh4Embree(const imba::TriangleMesh* mesh, int nrays);
    ~BenchRayBvh4Embree();

    float tmin() const { return tmin_; }
    float tmax() const { return tmax_; }
    int intr_count() const { return icount_; }

protected:
    virtual void iteration() override;
    virtual void display() override;

private:
    RTCScene scene_;
    unsigned mesh_id_;
    int nrays_, icount_;
    float tmin_, tmax_;
};

class BenchRay4Bvh4Embree : public Bench {
public:
    BenchRay4Bvh4Embree(const imba::TriangleMesh* mesh, int nray4s);
    ~BenchRay4Bvh4Embree();

    float tmin() const { return tmin_; }
    float tmax() const { return tmax_; }
    int intr_count() const { return icount_; }

protected:
    virtual void iteration() override;
    virtual void display() override;

private:
    RTCScene scene_;
    unsigned mesh_id_;
    int nray4s_, icount_;
    float tmin_, tmax_;
};

} // namespace bench

#endif // BENCH_RAY_BVH_HPP

