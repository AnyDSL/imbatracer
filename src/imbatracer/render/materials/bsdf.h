#ifndef IMBA_BSDF_H
#define IMBA_BSDF_H

#include "../../float4.h"
#include "../random.h"

namespace imba {

/// Base class for all BRDF and BTDF classes.
/// Defines the common interface that all of them support.
/// All direction vectors are in shading space, which means the normal is aligned with the z-axis.
class BxDF {
public:
    enum Flags {
        Reflection      = 1 << 0,
        Transmission    = 1 << 1,

        Diffuse         = 1 << 2,
        Glossy          = 1 << 3,
        Specular        = 1 << 4,

        AllTypes        = Diffuse | Glossy | Specular,

        AllReflection   = Reflection   | AllTypes,
        AllTransmission = Transmission | AllTypes,

        All = Reflection | Transmission | AllTypes
    };

    const Flags flags;

    BxDF(Flags f) : flags(f) {}

    bool matches_flags(Flags f) const { return (flags & f) == f; }

    virtual float4 eval(const float3& out_dir, const float3& in_dir) const = 0;
    virtual float4 sample(const float3& out_dir, float3& in_dir, float rnd_num_1, float rnd_num_2, float& pdf) const = 0;
    virtual float pdf(const float3& out_dir, const float3& in_dir) const = 0;
};


// Functions that compute angles in the shading coordinate system.
inline float cos_theta(const float3& dir) { return dir.z; }
inline float sin_theta_sqr(const float3& dir) { return std::max(0.0f, 1.0f - sqr(cos_theta(dir))); }
inline float sin_theta(const float3& dir) { return sqrtf(sin_theta_sqr(dir)); }

inline float cos_phi(const float3& dir) {
    float st = sin_theta(dir);
    if (st == 0.0f) return 1.0f;
    return clamp(dir.x / st, -1.0f, 1.0f);
}

inline float sin_phi(const float3& dir) {
    float st = sin_theta(dir);
    if (st == 0.0f) return 0.0f;
    return clamp(dir.y / st, -1.0f, 1.0f);
}

/// Combines multiple BRDFs and BTDFs into a single BSDF.
class BSDF {
public:
    float4 eval(const float3& out_dir, const float3& in_dir) const {

    }

    float4 sample(const float3& out_dir, float3& in_dir, float rnd_num_component, float rnd_num_1, float rnd_num_2,
                  BxDF::Flags flags, BxDF::Flags& sampled_flags, float& pdf) const {

    }

    float pdf(const float3& out_dir, const float3& in_dir, BxDF::Flags flags = BxDF::Flags::All) {

    }
};

}

#endif