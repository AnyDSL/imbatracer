#ifndef BENCH_RAY_TRIANGLE_HPP
#define BENCH_RAY_TRIANGLE_HPP

#include "../impala/impala_interface.h"
#include "../common/memory.hpp"
#include "bench.hpp"

namespace bench {

class BenchRayTriangleImpala : public Bench {
public:
    BenchRayTriangleImpala(int nrays)
        : Bench("bench_ray_triangle_impala")
        , result_(imba::thorin_make_unique<BenchRayTriangleResult>())
        , nrays_(nrays)
    {}

    float get_tmin() const { return tmin_; }
    int get_intr_count() const { return icount_; }

protected:
    virtual void iteration();
    virtual void display();

private:
    imba::ThorinUniquePtr<BenchRayTriangleResult> result_;
    int nrays_, icount_;
    float tmin_;
};

class BenchRay4TriangleImpala : public Bench {
public:
    BenchRay4TriangleImpala(int nrays)
        : Bench("bench_ray4_triangle_impala")
        , result_(imba::thorin_make_unique<BenchRayTriangleResult>())
        , nrays_(nrays)
    {}

    float get_tmin() const { return tmin_; }
    int get_intr_count() const { return icount_; }

protected:
    virtual void iteration();
    virtual void display();

private:
    imba::ThorinUniquePtr<BenchRayTriangleResult> result_;
    int nrays_, icount_;
    float tmin_;
};

class BenchRayTriangleEmbree : public Bench {
public:
    BenchRayTriangleEmbree(int nrays)
        : Bench("bench_ray_triangle_embree")
        , nrays_(nrays)
    {}

    float get_tmin() const { return tmin_; }
    int get_intr_count() const { return icount_; }

protected:
    virtual void iteration();
    virtual void display();

private:
    int nrays_, icount_;
    float tmin_;
};

class BenchRay4TriangleEmbree : public Bench {
public:
    BenchRay4TriangleEmbree(int nray4s)
        : Bench("bench_ray4_triangle_embree")
        , nray4s_(nray4s)
    {}

    float get_tmin() const { return tmin_; }
    int get_intr_count() const { return icount_; }

protected:
    virtual void iteration();
    virtual void display();

private:
    int nray4s_, icount_;
    float tmin_;
};

} // namespace bench

#endif

