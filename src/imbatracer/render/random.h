#ifndef IMBA_RANDOM_H
#define IMBA_RANDOM_H

#include "../core/float3.h"
#include "../core/common.h"
#include <random>

namespace imba {

class RNG {
public:
    RNG() {
        std::random_device rd;
        rng_ = std::mt19937(rd());
    }
    
    RNG(const RNG& rhs) = delete;
    RNG& operator= (const RNG&& rhs) = delete;
    
    float random(float min, float max) {
        std::uniform_real_distribution<float> uniform(min, max);
        return uniform(rng_);
    }
    
    float random01() { return random(0.0f, 1.0f); }
    
    int random(int min, int max) {
        std::uniform_int_distribution<int> uniform(min, max);
        return uniform(rng_);
    }
    
private:
    std::mt19937 rng_;
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
    float3 tangent;
    float3 binormal;
    local_coordinates(n, tangent, binormal);
    
    return generate_cosine_weighted_direction(n, tangent, binormal, u1, u2);
}

}

#endif
