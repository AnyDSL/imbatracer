#include "shader.h"
#include <float.h>
#include "../core/float4.h"

void imba::BasicPathTracer::operator()(Ray* rays, Hit* hits, int ray_count, Image& out) {
    const float trange = 20.0f;

//#pragma omp parallel for
    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id != -1) {
            out.pixels()[i * 4] = 1.0f - hits[i].tmax / trange;
            out.pixels()[i * 4 + 1] = 1.0f - hits[i].tmax / trange;
            out.pixels()[i * 4 + 2] = 1.0f - hits[i].tmax / trange;
        } else
            out.pixels()[i * 4] = 0.5f;
    }
}
