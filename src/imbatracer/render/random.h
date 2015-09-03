#ifndef IMBA_RANDOM_H
#define IMBA_RANDOM_H

#include "../core/float3.h"
#include "../core/common.h"

namespace imba {

float3 sample_hemisphere(const float3& n, float u1, float u2) {
    float theta = acosf(sqrtf(1.0f - u1));
    float phi = 2.0f * pi * u2;

    float xs = sinf(theta) * cosf(phi);
    float ys = cosf(theta);
    float zs = sinf(theta) * sinf(phi);

    float3 y = n;
    float3 h = y;
    if (fabsf(h.x) <= fabsf(h.y) && fabsf(h.x) <= fabsf(h.z)) {
        h.x = 1.0f;
    } else if (fabsf(h.y) <= fabsf(h.x) && fabsf(h.y) <= fabsf(h.z)) {
        h.y = 1.0f;
    } else {
        h.z = 1.0f;
    }

    float3 x = normalize(cross(h, y));
    float3 z = cross(x, y);

    return x * xs + y * ys + z * zs;
}

}

#endif
