#define _X86INTRIN_H_INCLUDED
//#include <geometry/box1_intersector1_moeller.h>
//#include <geometry/box1_intersector4_moeller.h>
#include "bench_ray_box.hpp"

#define COUNT 4000000

struct bench_ray_box_result
{
    int intr_count;
    float tmin;
    float tmax;
};

extern "C" bench_ray_box_result* bench_ray_box(int, const float*, const float*);
extern "C" bench_ray_box_result* bench_ray4_box(int, const float*, const float*);

namespace bench {

void BenchRayBoxImpala::iteration()
{
    const float min[3] = {0.2f, 0.0f, 0.0f};
    const float max[3] = {1.2f, 1.0f, 1.0f};

    bench_ray_box_result* result = bench_ray_box(nrays_, min, max);

    icount_ = result->intr_count;
    tmin_ = result->tmin;
    tmax_ = result->tmax;
    
    printf("%d %f %f\n", icount_, tmin_, tmax_);

    free(result);
}

void BenchRay4BoxImpala::iteration()
{
    const float min[3] = {0.2f, 0.0f, 0.0f};
    const float max[3] = {1.2f, 1.0f, 1.0f};

    bench_ray_box_result* result = bench_ray4_box(nrays_, min, max);

    icount_ = result->intr_count;
    tmin_ = result->tmin;
    tmax_ = result->tmax;
    
    printf("%d %f %f\n", icount_, tmin_, tmax_);

    free(result);
}

} // namespace bench

