#ifndef BENCH_RAY_BOX_HPP
#define BENCH_RAY_BOX_HPP

#include "bench.hpp"

namespace bench {

class BenchRayBoxImpala : public Bench {
public:
    BenchRayBoxImpala(int nrays)
        : Bench("bench_ray_box_impala")
        , nrays_(nrays)
    {}

    virtual void iteration();

    float get_tmin() const { return tmin_; }
    float get_tmax() const { return tmax_; }
    int get_intr_count() const { return icount_; }

private:
    int nrays_, icount_;
    float tmin_, tmax_;
};

class BenchRay4BoxImpala : public Bench {
public:
    BenchRay4BoxImpala(int nrays)
        : Bench("bench_ray4_box_impala")
        , nrays_(nrays)
    {}

    virtual void iteration();

    float get_tmin() const { return tmin_; }
    float get_tmax() const { return tmax_; }
    int get_intr_count() const { return icount_; }

private:
    int nrays_, icount_;
    float tmin_, tmax_;
};

} // namespace bench

#endif

