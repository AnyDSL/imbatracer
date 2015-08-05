#include "shader.h"
#include <float.h>
#include "../core/float4.h"

void imba::BasicPathTracer::operator()(Ray* rays, Hit* hits, int ray_count, Image& out) {
    const float trange = 100.0f;

#pragma omp parallel for
    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tmax < FLT_MAX) {
            out.pixels()[i * 4] = hits[i].tmax / trange;
        }
    }
}
