#ifndef BENCH_RAY_BOX_HPP
#define BENCH_RAY_BOX_HPP

#include "impala/impala_interface.h"
#include "common/memory.hpp"
#include "bench.hpp"

namespace bench {

class BenchRayBoxImpala : public Bench {
public:
    BenchRayBoxImpala(int nrays)
        : Bench("bench_ray_box_impala")
        , result_(imba::thorin_make_unique<BenchRayBoxResult>())
        , nrays_(nrays)
    {}

    float tmin() const { return tmin_; }
    float tmax() const { return tmax_; }
    int intr_count() const { return icount_; }

protected:
    virtual void iteration() override;
    virtual void display() override;

private:
    imba::ThorinUniquePtr<BenchRayBoxResult> result_;
    int nrays_, icount_;
    float tmin_, tmax_;
};

class BenchRay4BoxImpala : public Bench {
public:
    BenchRay4BoxImpala(int nrays)
        : Bench("bench_ray4_box_impala")
        , result_(imba::thorin_make_unique<BenchRayBoxResult>())
        , nrays_(nrays)
    {}

    float tmin() const { return tmin_; }
    float tmax() const { return tmax_; }
    int intr_count() const { return icount_; }

protected:
    virtual void iteration() override;
    virtual void display() override;

private:
    imba::ThorinUniquePtr<BenchRayBoxResult> result_;
    int nrays_, icount_;
    float tmin_, tmax_;
};

} // namespace bench

#endif // BENCH_RAY_BOX_HPP

