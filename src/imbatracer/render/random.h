#ifndef IMBA_RANDOM_H
#define IMBA_RANDOM_H

#include "../core/float3.h"
#include "../core/common.h"
#include <random>

namespace imba {

/// Small and fast random number generator based on MWC64X
/// http://cas.ee.ic.ac.uk/people/dt10/research/rngs-gpu-mwc64x.html
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

    // Random number from min (inclusive) to max (exclusive)
    int random_int(int min, int max) {
        return MWC64X() % (max - min) + min;
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

struct DirectionSample {
    float3 dir;
    float pdf;

    DirectionSample(float3 dir, float pdf) : dir(dir), pdf(pdf) {}
};

/// Computes an orthogonal local coordinate system.
inline void local_coordinates(const float3& normal, float3& tangent_out, float3& binormal_out) {
    const int id0  = (fabsf(normal.x) > fabsf(normal.y)) ? 0 : 1;
    const int id1  = (fabsf(normal.x) > fabsf(normal.y)) ? 1 : 0;
    const float sig = (fabsf(normal.x) > fabsf(normal.y)) ? -1.f : 1.f;

    const float inv_len = 1.f / (normal[id0] * normal[id0] + normal.z * normal.z);

    tangent_out[id0] = normal.z * sig * inv_len;
    tangent_out[id1] = 0.f;
    tangent_out.z   = normal[id0] * -1.f * sig * inv_len;

    tangent_out = normalize(tangent_out);

    binormal_out = cross(normal, tangent_out);
}

inline float3 spherical_dir(float sintheta, float costheta, float phi) {
    return float3(sintheta * cosf(phi),
                  sintheta * sinf(phi),
                  costheta);
}

inline DirectionSample sample_cos_hemisphere(float u1, float u2) {
    const float3 local_dir = spherical_dir(sqrtf(1 - u2), sqrtf(u2), 2.f * pi * u1);
    return DirectionSample(local_dir, local_dir.z * 1.0f / pi);
}

inline float cos_hemisphere_pdf(const float3& dir) {
    return std::max(0.0f, dir.z) * 1.0f / pi;
}

inline DirectionSample sample_power_cos_hemisphere(float power, float u1, float u2) {
    const float phi = pi * 2.0f * u1;
    const float cos_t = powf(u2, 1.0f / (power + 1.0f));
    const float sin_t = sqrtf(1.0f - sqr(cos_t)); // cos_t cannot be >= 1

    const float3 local_dir = spherical_dir(sin_t, cos_t, phi);

    return DirectionSample(local_dir, (power + 1.0f) * powf(cos_t, power) * 1.0f / (2.0f * pi));
}

inline float power_cos_hemisphere_pdf(float power, const float3& dir) {
    return dir.z > 0.0f ? ((power + 1.0f) * powf(dir.z, power) * 1.0f / (2.0f * pi)) : 0.0f;
}

inline DirectionSample sample_uniform_hemisphere(float u1, float u2) {
    const float phi = 2.f * pi * u1;

    const float3 local_dir(
        cosf(phi) * sqrtf(1.0f - u2 * u2),
        sinf(phi) * sqrtf(1.0f - u2 * u2),
        u2);

    return DirectionSample(local_dir, 1.0f / (2.0f * pi));
}

inline float uniform_hemisphere_pdf(const float3& dir) {
    return 1.0f / (2.0f * pi);
}

inline void sample_uniform_triangle(float rnd1, float rnd2, float& u, float& v) {
    float sqrt_rnd1 = sqrtf(rnd1);
    u = 1.0f - sqrt_rnd1;
    v = rnd2 * sqrt_rnd1;
}

inline DirectionSample sample_uniform_sphere(float u1, float u2) {
    const float a = 2.0f * pi * u1;
    const float b = 2.0f * sqrtf(u2 - sqr(u2));

    const float3 dir(cosf(a) * b,
        sinf(a) * b,
        1.0f - 2.0f * u2);

    return DirectionSample(dir, 1.0f / (4.0f * pi));
}

inline float uniform_sphere_pdf() {
    return 1.0f / (4.0f * pi);
}

inline float2 sample_concentric_disc(float u1, float u2) {
    // Taken from SmallVCM

    float phi, r;

    float a = 2 * u1 - 1;
    float b = 2 * u2 - 1;

    if(a > -b) {
        if(a > b) {
            r = a;
            phi = (pi * 0.25f) * (b / a);
        } else {
            r = b;
            phi = (pi * 0.25f) * (2.f - (a / b));
        }
    } else {
        if(a < b) {
            r = -a;
            phi = (pi * 0.25f) * (4.f + (b / a));
        } else {
            r = -b;

            if (b != 0)
                phi = (pi * 0.25f) * (6.f - (a / b));
            else
                phi = 0;
        }
    }

    return float2(r * cosf(phi), r * sinf(phi));
}

inline float concentric_disc_pdf() {
    return 1.0f / pi;
}

}

#endif
