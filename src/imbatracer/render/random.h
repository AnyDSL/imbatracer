#ifndef IMBA_RANDOM_H
#define IMBA_RANDOM_H

#include "../core/float3.h"
#include "../core/common.h"
#include <random>

namespace imba {

class RNG {
public:
    RNG(uint64_t seed = 0) : state_(seed) {}

    float random_float(float min, float max) {
        const float r = random_float();
        return lerp(min, max, r);
    }

    float random_float() {
        return static_cast<float>(MWC64X()) / static_cast<float>(0xFFFFFFFF);
    }

    int random_int(int min, int max) {
        const float r = random_float();
        return lerp(min, max, r);
    }

private:
    uint64_t state_;

    uint32_t MWC64X() {
        const uint32_t c = (state_) >> 32;
        const uint32_t x = (state_) & 0xFFFFFFFF;
        state_ = x * ((uint64_t)4294883355U) + c;
        return x^c;
    }
};

//computes orthogonal local coordinate system
inline void local_coordinates(const float3& normal, float3& tangent_out, float3& binormal_out) {
    const int id0  = (fabsf(normal.x) > fabsf(normal.y)) ? 0 : 1;
    const int id1  = (fabsf(normal.x) > fabsf(normal.y)) ? 1 : 0;
    const float sig = (fabsf(normal.x) > fabsf(normal.y)) ? -1.f : 1.f;

    const float inv_len = 1.f / (normal[id0] * normal[id0] + normal.z * normal.z);

    tangent_out[id0] = normal.z * sig * inv_len;
    tangent_out[id1] = 0.f;
    tangent_out.z   = normal[id0] * -1.f * sig * inv_len;

    binormal_out = cross(normal, tangent_out);
}

struct DirectionSample {
    float3 dir;
    float pdf;
    
    DirectionSample(float3 dir, float pdf) : dir(dir), pdf(pdf) {}
};

inline DirectionSample generate_cosine_weighted_direction(const float3& up, const float3& left, const float3& forward, const float u1, const float u2) {
    float3 dir(
        cosf(2.f * pi * u1) * sqrtf(1 - u2),
        sinf(2.f * pi * u1) * sqrtf(1 - u2),
        sqrtf(u2));

    dir = dir.x * left + dir.y * forward + dir.z * up;

    return DirectionSample(dir, 1.f / pi);
}

inline DirectionSample sample_hemisphere(const float3& n, float u1, float u2) {
    assert_normalized(n);

    float3 tangent;
    float3 binormal;
    local_coordinates(n, tangent, binormal);
    
    return generate_cosine_weighted_direction(n, tangent, binormal, u1, u2);
}

inline void uniform_sample_triangle(float rnd1, float rnd2, float& u, float& v) {
    float sqrt_rnd1 = sqrtf(rnd1);
    u = 1.0f - sqrt_rnd1;
    v = rnd2 * sqrt_rnd1;
}

}

#endif
