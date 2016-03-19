#ifndef IMBA_BSDF_H
#define IMBA_BSDF_H

#include "../../core/float4.h"

#include "../mem_arena.h"
#include "../random.h"
#include "../intersection.h"

#include "fresnel.h"

#include <cassert>

namespace imba {

enum BxDFFlags {
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

    virtual float4 eval(const float3& out_dir, const float3& in_dir) const = 0;

    /// Default implementation cosine-samples the hemisphere.
    virtual float4 sample(const float3& out_dir, float3& in_dir, float rnd_num_1, float rnd_num_2, float& pdf) const {
        DirectionSample ds = sample_cos_hemisphere(rnd_num_1, rnd_num_2);
        in_dir = ds.dir;
        pdf = ds.pdf;

        // If the out direction is on the other side (according to the normal)
        // we need to flip the sampled direction as well.
        if (out_dir.z < 0.0f) in_dir.z = -in_dir.z;

        return eval(out_dir, in_dir);
    }

    virtual float pdf(const float3& out_dir, const float3& in_dir) const {
        return same_hemisphere(out_dir, in_dir) ? cos_hemisphere_pdf(in_dir) : 0.0f;
    }
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
    BSDF(const Intersection& isect) : isect_(isect), num_bxdfs_(0) {
        // Compute base vectors of the shading space.
        local_coordinates(isect.normal, tangent_, binormal_);
    }

    void add(BxDF* b) {
        assert(num_bxdfs_ < MAX_BXDFS);
        bxdfs_[num_bxdfs_++] = b;
    }

    int count() const { return num_bxdfs_; }

    int count(BxDFFlags flags) const {
        int nr = 0;
        for (int i = 0; i < count(); ++i) {
            if (bxdfs_[i]->matches_flags(flags))
                nr++;
        }
        return nr;
    }

    float4 eval(const float3& out_dir, const float3& in_dir, BxDFFlags flags = BSDF_ALL) const {
        float3 local_out = world_to_local(out_dir);
        float3 local_in = world_to_local(in_dir);

        // Some care has to be taken when using shading normals to prevent light leaks and dark spots.
        // We follow the approach from PBRT and use the geometric normal to decide whether to evaluate
        // the BRDF (reflection) or the BTDF (transmission.)
        if (dot(in_dir, isect_.geom_normal) * dot(out_dir, isect_.geom_normal) <= 0.0f /*||
            dot(in_dir, isect_.normal)      * dot(out_dir, isect_.normal)      <= 0.0f*/)
            flags = BxDFFlags(flags & ~BSDF_REFLECTION); // in and out are on different sides: ignore reflection
        else
            flags = BxDFFlags(flags & ~BSDF_TRANSMISSION); // in and out are on the same side: ignore transmission

        float4 res(0.0f);
        for (int i = 0; i < num_bxdfs_; ++i)
            if (bxdfs_[i]->matches_flags(flags))
                res += bxdfs_[i]->eval(local_out, local_in);
        return res;
    }

    float4 sample(const float3& out_dir, float3& in_dir, float rnd_num_component, float rnd_num_1, float rnd_num_2,
                  BxDFFlags flags, BxDFFlags& sampled_flags, float& pdf) const {
        // Choose a BxDF to sample from all BxDFs that match the given flags.
        int num_matching_bxdf = count(flags);
        if (num_matching_bxdf == 0) {
            pdf = 0.0f;
            return float4(0.0f);
        }

        int chosen_i = std::min(static_cast<int>(rnd_num_component * num_matching_bxdf), num_matching_bxdf - 1);

        BxDF* chosen_bxdf = nullptr;
        int counter = 0;
        for (int i = 0; i < count(); ++i) {
            if (bxdfs_[i]->matches_flags(flags) && counter++ == chosen_i) {
                chosen_bxdf = bxdfs_[i];
            }
        }

        // Sample the BxDF
        float3 local_out = world_to_local(out_dir);
        float3 local_in;

        float4 value = chosen_bxdf->sample(local_out, local_in, rnd_num_1, rnd_num_2, pdf);

        if (pdf == 0.0f) {
            sampled_flags = BxDFFlags(0);
            return float4(0.0f);
        }

        sampled_flags = chosen_bxdf->flags;
        in_dir = local_to_world(local_in);

        // Add the pdfs of all other matching BxDFs as well,
        // except if we sampled from a delta destribution, because we represent delta distributions with a pdf value of one.
        if (!(sampled_flags & BSDF_SPECULAR) && num_matching_bxdf > 1) {
            for (int i = 0; i < count(); ++i) {
                if (i != chosen_i && bxdfs_[i]->matches_flags(flags)) {
                    pdf += bxdfs_[i]->pdf(local_out, local_in);
                }
            }
        }

        if (num_matching_bxdf > 1)
            pdf /= num_matching_bxdf;

        // Compute the BxDF value unless it was represented by a delta distribution.
        // Once more, we take all matching BxDFs into account.
        if (!(sampled_flags & BSDF_SPECULAR)) {
            // Basically the same as eval, but without calculating the local coordinates again
            // and without recalculating the value of the sampled BxDF.

            if (dot(in_dir, isect_.geom_normal) * dot(out_dir, isect_.geom_normal) <= 0.0f /*||
                dot(in_dir, isect_.normal)      * dot(out_dir, isect_.normal)      <= 0.0f*/)
                flags = BxDFFlags(flags & ~BSDF_REFLECTION); // in and out are on different sides: ignore reflection
            else
                flags = BxDFFlags(flags & ~BSDF_TRANSMISSION); // in and out are on the same side: ignore transmission

            for (int i = 0; i < num_bxdfs_; ++i)
                if (i != chosen_i && bxdfs_[i]->matches_flags(flags))
                    value += bxdfs_[i]->eval(local_out, local_in);
        }

        return value;
    }

    /// Computes the pdf of sampling the given incoming direction, taking into account only BxDFs with the given flags.
    float pdf(const float3& out_dir, const float3& in_dir, BxDFFlags flags = BSDF_ALL) const {
        float3 local_out = world_to_local(out_dir);
        float3 local_in = world_to_local(in_dir);

        float pdf = 0.0f;
        int num_matching_bxdf = 0;
        for (int i = 0; i < count(); ++i) {
            if (bxdfs_[i]->matches_flags(flags)) {
                pdf += bxdfs_[i]->pdf(local_out, local_in);
                ++num_matching_bxdf;
            }
        }

        pdf = num_matching_bxdf > 0 ? pdf / num_matching_bxdf : 0.0f;
        return pdf;
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

    static constexpr int MAX_BXDFS = 8;
    int num_bxdfs_;
    BxDF* bxdfs_[MAX_BXDFS];
};

}

#endif