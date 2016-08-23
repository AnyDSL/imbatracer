#ifndef IMBA_BSDF_H
#define IMBA_BSDF_H

#include "../../core/rgb.h"

#include "../mem_arena.h"
#include "../random.h"
#include "../intersection.h"

#include "fresnel.h"

#include <cassert>

namespace imba {

enum BxDFFlags {
    BSDF_NONE = 0,

    BSDF_REFLECTION      = 1 << 0,
    BSDF_TRANSMISSION    = 1 << 1,

    BSDF_DIFFUSE         = 1 << 2,
    BSDF_GLOSSY          = 1 << 3,
    BSDF_SPECULAR        = 1 << 4,

    BSDF_ALLTYPES        = BSDF_DIFFUSE | BSDF_GLOSSY | BSDF_SPECULAR,

    BSDF_ALL_REFLECTION   = BSDF_REFLECTION   | BSDF_ALLTYPES,
    BSDF_ALL_TRANSMISSION = BSDF_TRANSMISSION | BSDF_ALLTYPES,

    BSDF_ALL = BSDF_REFLECTION | BSDF_TRANSMISSION | BSDF_ALLTYPES,

    BSDF_NON_SPECULAR = BSDF_REFLECTION | BSDF_TRANSMISSION | BSDF_DIFFUSE | BSDF_GLOSSY
};

inline bool same_hemisphere(const float3& out_dir, const float3& in_dir) {
    return out_dir.z * in_dir.z > 0.0f;
}

/// Base class for all BRDF and BTDF classes.
/// Defines the common interface that all of them support.
/// All direction vectors are in shading space, which means the normal is aligned with the z-axis.
class BxDF {
public:
    const BxDFFlags flags;

    BxDF(BxDFFlags f) : flags(f) {}

    bool matches_flags(BxDFFlags f) const { return (flags & f) == flags; }

    virtual rgb eval(const float3& out_dir, const float3& in_dir) const = 0;

    /// Default implementation cosine-samples the hemisphere.
    virtual rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const {
        DirectionSample ds = sample_cos_hemisphere(rng.random_float(), rng.random_float());
        in_dir = ds.dir;
        pdf = ds.pdf;

        // If the out direction is on the other side (according to the normal)
        // we need to flip the sampled direction as well.
        if (out_dir.z < 0.0f) in_dir.z = -in_dir.z;

        return eval(out_dir, in_dir);
    }

    virtual float pdf(const float3& out_dir, const float3& in_dir) const {
        return same_hemisphere(out_dir, in_dir) ? cos_hemisphere_pdf(in_dir.z) : 0.0f;
    }

    /// Returns the desired probability for importance sampling when choosing between BRDF and BTDF.
    /// Only meaningful for BTDFs, ignored for BRDFs.
    virtual float importance(const float3& out_dir) const { return 0.5f; }
};

class CombineBxDF : public BxDF {
public:
    CombineBxDF(BxDF* a, BxDF* b) : BxDF(BxDFFlags(a->flags | b->flags)), a_(a), b_(b) {}

    rgb eval(const float3& out_dir, const float3& in_dir) const override {
        return 0.5f * (a_->eval(out_dir, in_dir) + b_->eval(out_dir, in_dir));
    }

    /// Default implementation cosine-samples the hemisphere.
    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const override {
        return (rng.random_float() < 0.5f ? a_ : b_)->sample(out_dir, in_dir, rng, pdf);
    }

    float pdf(const float3& out_dir, const float3& in_dir) const override {
        // Probability to sample in_dir = probability for a to sample in_dir * probability to choose a for sampling
        //                              + probability for b to sample in_dir * probability to choose b for sampling
        return (a_->pdf(out_dir, in_dir) + b_->pdf(out_dir, in_dir)) * 0.5f;
    }

private:
    BxDF* a_;
    BxDF* b_;
};

// Functions that compute angles in the shading coordinate system.
inline float cos_theta(const float3& dir) { return dir.z; }
inline float abs_cos_theta(const float3& dir) { return fabsf(dir.z); }
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
    /// Initializes the BSDF for the given surface point.
    BSDF(const Intersection& isect, BxDF* brdf, BxDF* btdf)
        : isect_(isect), brdf_(brdf), btdf_(btdf), tangent_(isect.u_tangent), binormal_(isect.v_tangent) {
    }

    rgb eval(const float3& out_dir, const float3& in_dir, BxDFFlags flags = BSDF_ALL) const {
        float3 local_out = world_to_local(out_dir);
        float3 local_in = world_to_local(in_dir);

        // Some care has to be taken when using shading normals to prevent light leaks and dark spots.
        // We follow the approach from PBRT and use the geometric normal to decide whether to evaluate
        // the BRDF (reflection) or the BTDF (transmission.)
        BxDF* chosen_bxdf;
        if (dot(in_dir, isect_.geom_normal) * dot(out_dir, isect_.geom_normal) <= 0.0f)
            chosen_bxdf = btdf_; // in and out are on different sides: ignore reflection
        else
            chosen_bxdf = brdf_; // in and out are on the same side: ignore transmission

        if (chosen_bxdf == nullptr)
            return rgb(0.0f);

        rgb res(0.0f);
        if (chosen_bxdf->matches_flags(flags))
            res = chosen_bxdf->eval(local_out, local_in);
        return res;
    }

    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, BxDFFlags flags, BxDFFlags& sampled_flags, float& pdf) const {
        float3 local_out = world_to_local(out_dir);

        // Select which to sample: BRDF or BTDF, based on importance specified by the BTDF.
        const bool brdf_matches = brdf_ ? brdf_->matches_flags(flags) : false;
        const bool btdf_matches = btdf_ ? btdf_->matches_flags(flags) : false;

        float btdf_prob;
        if (brdf_matches && btdf_matches)
            btdf_prob = btdf_->importance(local_out);
        else if (brdf_matches)
            btdf_prob = 0.0f;
        else if (btdf_matches)
            btdf_prob = 1.0f;
        else {
            pdf = 0.0f;
            sampled_flags = BxDFFlags(0);
            return rgb(0.0f);
        }

        BxDF* chosen_bxdf;
        float component_pdf;
        const float rnd_num_component = rng.random_float();
        if (rnd_num_component < btdf_prob) {
            chosen_bxdf = btdf_;
            component_pdf = btdf_prob;
        } else {
            chosen_bxdf = brdf_;
            component_pdf = 1.0f - btdf_prob;
        }

        // Sample the BxDF
        float3 local_in;
        rgb value = chosen_bxdf->sample(local_out, local_in, rng, pdf);

        if (pdf == 0.0f) {
            sampled_flags = BxDFFlags(0);
            return rgb(0.0f);
        }

        sampled_flags = chosen_bxdf->flags;
        in_dir = local_to_world(local_in);
        pdf *= component_pdf;

        // Ensure that BRDF samples are always in the same hemisphere, and that BTDF samples are always in the opposite hemisphere.
        if ((chosen_bxdf == brdf_ && dot(in_dir, isect_.geom_normal) * dot(out_dir, isect_.geom_normal) <= 0.0f) ||
            (chosen_bxdf == btdf_ && dot(in_dir, isect_.geom_normal) * dot(out_dir, isect_.geom_normal) >= 0.0f)) {
            sampled_flags = BxDFFlags(0);
            return rgb(0.0f);
        }

        return value;
    }

    /// Computes the pdf of sampling the given incoming direction, taking into account only BxDFs with the given flags.
    float pdf(const float3& out_dir, const float3& in_dir, BxDFFlags flags = BSDF_ALL) const {
        float3 local_out = world_to_local(out_dir);
        float3 local_in = world_to_local(in_dir);

        BxDF* chosen_bxdf;
        if (dot(in_dir, isect_.geom_normal) * dot(out_dir, isect_.geom_normal) <= 0.0f)
            chosen_bxdf = btdf_;
        else
            chosen_bxdf = brdf_;

        if (chosen_bxdf == nullptr)
            return 0.0f;

        if (chosen_bxdf->matches_flags(flags))
            return chosen_bxdf->pdf(local_out, local_in);

        return 0.0f;
    }

    float3 world_to_local(const float3& dir) const {
        return float3(dot(binormal_, dir),
                      dot(tangent_, dir),
                      dot(isect_.normal, dir));
    }

    float3 local_to_world(const float3& dir) const {
        return float3(binormal_.x * dir.x + tangent_.x * dir.y + isect_.normal.x * dir.z,
                      binormal_.y * dir.x + tangent_.y * dir.y + isect_.normal.y * dir.z,
                      binormal_.z * dir.x + tangent_.z * dir.y + isect_.normal.z * dir.z);
    }

private:
    const Intersection& isect_;
    float3 tangent_;
    float3 binormal_;

    BxDF* brdf_;
    BxDF* btdf_;
};

} // namespace imba

#endif
