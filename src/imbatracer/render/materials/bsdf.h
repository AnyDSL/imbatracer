#ifndef IMBA_BSDF_H
#define IMBA_BSDF_H

#include "imbatracer/core/rgb.h"

#include "imbatracer/render/mem_arena.h"
#include "imbatracer/render/random.h"
#include "imbatracer/render/intersection.h"

#include "imbatracer/render/materials/fresnel.h"

#include <cassert>
#include <algorithm>

namespace imba {

/// Base class for all BRDF and BTDF classes.
/// Defines the common interface that all of them support.
/// All direction vectors are in shading space, which means the normal is aligned with the z-axis.
class BxDF {
public:
    BxDF(const float3& normal) : normal(normal) {}

    /// Returns the value of the BSDF times cosine of the incoming direction.
    virtual rgb eval(const float3& out_dir, const float3& in_dir) const = 0;

    /// Samples a direction from the BSDF, returns the value for that direction times cosine divided by the pdf.
    virtual rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const {
        DirectionSample ds = sample_cos_hemisphere(rng.random_float(), rng.random_float());
        in_dir = local_to_world(ds.dir, dot(out_dir, normal));
        pdf = ds.pdf;
        return eval(out_dir, in_dir) / pdf;
    }

    virtual float pdf(const float3& out_dir, const float3& in_dir) const {
        float c = dot(normal, in_dir);
        return c * dot(normal, out_dir) > 0 ? cos_hemisphere_pdf(c) : 0.0f;
    }

    virtual float albedo(const float3& out_dir) const { return 1.0f; }

    virtual bool specular() const { return false; }

    float3 normal;

    float3 local_to_world(const float3& dir, float hemisphere) const {
        float3 tangent, binormal;
        float3 n = hemisphere < 0.0f ? -normal : normal;
        local_coordinates(n, tangent, binormal);
        return float3(tangent.x * dir.x + binormal.x * dir.y + n.x * dir.z,
                      tangent.y * dir.x + binormal.y * dir.y + n.y * dir.z,
                      tangent.z * dir.x + binormal.z * dir.y + n.z * dir.z);
    }

    inline bool same_hemisphere(const float3& out_dir, const float3& in_dir) const {
        return cos_theta(out_dir) * cos_theta(in_dir) > 0.0f;
    }

    inline float cos_theta(const float3& dir) const { return dot(normal, dir); }
    inline float abs_cos_theta(const float3& dir) const { return fabsf(cos_theta(dir)); }
    inline float sin_theta_sqr(const float3& dir) const { return std::max(0.0f, 1.0f - sqr(cos_theta(dir))); }
    inline float sin_theta(const float3& dir) const { return sqrtf(sin_theta_sqr(dir)); }

    inline float cos_phi(const float3& dir, const float3& tangent, const float3& binormal) const {
        float st = sin_theta(dir);
        if (st == 0.0f) return 1.0f;
        return clamp((tangent.x * dir.x + binormal.x * dir.y + normal.x * dir.z) / st, -1.0f, 1.0f);
    }

    inline float sin_phi(const float3& dir, const float3& tangent, const float3& binormal) const {
        float st = sin_theta(dir);
        if (st == 0.0f) return 0.0f;
        return clamp((tangent.y * dir.x + binormal.y * dir.y + normal.y * dir.z) / st, -1.0f, 1.0f);
    }

    inline float3 reflect(const float3& dir) const {
        return dir - 2.0f * dot(dir, normal) * normal;
    }
};

/// Combines multiple BRDFs and BTDFs into a single BSDF.
template <int MAX_COMPONENTS, int MAX_SIZE>
class CombineBSDF {
public:
    CombineBSDF()
        : all_specular_(true), num_comps_(0)
    { }

    CombineBSDF(const CombineBSDF&) = delete;
    CombineBSDF& operator= (const CombineBSDF&) = delete;

    void init(float3 geom_normal, bool adjoint) {
        geom_normal_ = geom_normal;
        adjoint_     = adjoint;
    }

    /// Checks wether or not this BSDF is a black body, i.e. absorbing all light.
    bool black_body() const {
        return num_comps_ == 0;
    }

    /// Adds a new BSDF to this combined BSDF
    template <typename T, typename... Args>
    bool add(const float3& weight, const Args&... a) {
        if (is_black(weight)) return true;

        // Ensure that no buffer overflow can occur.
        if (num_comps_ >= MAX_COMPONENTS) return false;

        weights_[num_comps_] = weight;
        components_[num_comps_] = new (mem_pool_ + num_comps_ * MAX_SIZE) T(a...);

        num_comps_++;

        if (!components_[num_comps_ - 1]->specular()) all_specular_ = false;

        return true;
    }

    /// Done adding components.
    /// The BSDF is now prepared for rendering.
    /// Calling add() after a call to this function results in undefined behavior.
    void prepare(const float3& throughput, const float3& out_dir) {
        float total = 0.0f;
        for (int i = 0; i < num_comps_; ++i) {
            // TODO taking the throuput into account requires some knowledge during VCM partial MIS
            // computation that is not available until after all paths have been traced!
            //pdfs_[i] = dot(weights_[i], throughput * components_[i]->albedo(out_dir));
            pdfs_[i] = luminance(weights_[i] * components_[i]->albedo(out_dir));
            total += pdfs_[i];
        }

        if (total == 0.0f) {
            all_specular_ = false; // everything is absorbed so no specular
            return;
        }

        // normalize the pdfs
        for (int i = 0; i < num_comps_; ++i) {
            pdfs_[i] /= total;
        }
    }

    rgb eval(const float3& out_dir, const float3& in_dir) const {
        rgb res(0.0f);
        for (int i = 0; i < num_comps_; ++i) {
            res += weights_[i] * components_[i]->eval(out_dir, in_dir) * shading_normal_correction(out_dir, in_dir, i);
        }

        return res;
    }

    /// Samples a direction from this BSDF
    /// \param specular Will be set to true if the chosen BSDF was specular. Does not mean that the entire combined BSDF is specular!
    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf, bool& specular) const {
        float rnd_comp = rng.random_float();
        specular = false;
        float sum = 0.0f;
        pdf = 0.0f;
        rgb res(0.0f);
        for (int i = 0; i < num_comps_; ++i) {
            if (rnd_comp < pdfs_[i] + sum) {
                float sample_pdf;
                res += weights_[i] * components_[i]->sample(out_dir, in_dir, rng, sample_pdf) * shading_normal_correction(out_dir, in_dir, i) / pdfs_[i];
                pdf = sample_pdf * pdfs_[i];

                // Evaluate the contributions of all other BSDFs to this direction
                for (int j = 0; j < num_comps_; ++j) {
                    if (i == j) continue;
                    res += weights_[j] * components_[j]->eval(out_dir, in_dir) * shading_normal_correction(out_dir, in_dir, j) / (sample_pdf * pdfs_[i]);
                    pdf += components_[j]->pdf(out_dir, in_dir) * pdfs_[j];
                }
                if (components_[i]->specular()) specular = true;
                break;
            }
            sum += pdfs_[i];
        }
        return res;
    }

    /// Computes the pdf of sampling the given incoming direction, taking into account only BxDFs with the given flags.
    float pdf(const float3& out_dir, const float3& in_dir) const {
        if (black_body()) return 0.0f;

        float pdf = 0.0f;
        float factor = 0.0f;
        for (int i = 0; i < num_comps_; ++i) {
            float select_pdf = luminance(weights_[i] * components_[i]->albedo(out_dir));
            pdf += select_pdf * components_[i]->pdf(out_dir, in_dir);
            factor += select_pdf;
        }
        return pdf / factor;
    }

    bool is_specular() const { return all_specular_; }

private:
    BxDF* components_[MAX_COMPONENTS];
    rgb   weights_[MAX_COMPONENTS];
    float pdfs_[MAX_COMPONENTS];
    char  mem_pool_[MAX_COMPONENTS * MAX_SIZE];
    int num_comps_;
    bool all_specular_;

    bool adjoint_;
    float3 geom_normal_;

    /// Applies the adjoint correction for the shading normals.
    float shading_normal_correction(const float3& out, const float3& in, int component) const {
        if (!adjoint_) return 1.0f;

        auto& normal = components_[component]->normal;
        
        float n = dot(normal, out) * dot(geom_normal_, in);
        float d = dot(normal, in)  * dot(geom_normal_, out);
        return fabsf(d) <= 0.001f ? 0.0f : fabsf(n / d);
    }
};

using BSDF = CombineBSDF<8, 256>;

} // namespace imba

#endif
