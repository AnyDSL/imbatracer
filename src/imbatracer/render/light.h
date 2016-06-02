#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "../core/float3.h"
#include "random.h"
#include <cfloat>
#include <memory>

namespace imba {

struct BoundingSphere {
    float3 center;
    float radius;
    float inv_radius_sqr; // Precomputed, needed for sampling emission from directional light sources.
};

class Light {
public:
    struct DirectIllumSample {
        float3 dir;
        float distance;

        float4 radiance;

        float cos_out;

        // Solid angle pdf for sampling this contribution during emission.
        float pdf_emit_w;
        // Solid angle pdf for sampling this contribution via direct illumination.
        float pdf_direct_w;
    };

    struct EmitSample {
        float3 pos;
        float3 dir;

        float4 radiance;

        float cos_out;

        // Solid angle pdf for sampling this contribution during emission.
        float pdf_emit_w;
        // Area pdf for sampling this contribution via direct illumination.
        float pdf_direct_a;
    };

    /// Samples an outgoing ray from the light source.
    virtual EmitSample sample_emit(RNG& rng) = 0;

    /// Samples a point on the light source. Used for shadow rays.
    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) = 0;

    /// Computes the amount of outgoing radiance from this light source in a given direction
    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) = 0;

    virtual bool is_delta() const { return false; }
    virtual bool is_finite() const { return true; }

    virtual ~Light() { }
};

class TriangleLight : public Light {
public:
    TriangleLight(float4 intensity, float3 p0, float3 p1, float3 p2) : intensity_(intensity), p0_(p0), p1_(p1), p2_(p2) {
        normal_ = cross(p1 - p0, p2 - p0);
        area_   = length(normal_) * 0.5f;
        normal_ = normalize(normal_);
        local_coordinates(normal_, tangent_, binormal_);
    }

    inline float area() const { return area_; }

    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        DirectIllumSample sample;

        // sample a point on the light source
        float u, v;
        sample_uniform_triangle(rng.random_float(), rng.random_float(), u, v);
        const float3 pos = u * p0_ + v * p1_ + (1.0f - u - v) * p2_;

        // compute distance and shadow ray direction
        sample.dir         = pos - from;
        const float distsq = dot(sample.dir, sample.dir);
        sample.distance    = sqrtf(distsq);
        sample.dir         = sample.dir * (1.0f / sample.distance);

        const float cos_out = dot(normal_, -1.0f * sample.dir);

        // directions form the opposite side of the light have zero intensity
        if (cos_out > 0.0f && cos_out < 1.0f) {
            sample.radiance = intensity_ * cos_out * (area_ / distsq);

            sample.cos_out      = cos_out;
            sample.pdf_emit_w   = (cos_out * 1.0f / pi) / area_;
            sample.pdf_direct_w = 1.0f / area_ * distsq / cos_out;
        } else {
            sample.radiance = float4(0.0f);

            // Prevent NaNs in the integrator
            sample.cos_out      = 1.0f;
            sample.pdf_emit_w   = 1.0f;
            sample.pdf_direct_w = 1.0f;
        }

        return sample;
    }

    virtual EmitSample sample_emit(RNG& rng) override {
        EmitSample sample;

        // Sample a point on the light source
        float u, v;
        sample_uniform_triangle(rng.random_float(), rng.random_float(), u, v);
        sample.pos = u * p0_ + v * p1_ + (1.0f - u - v) * p2_;

        // Sample an outgoing direction
        DirectionSample dir_sample = sample_cos_hemisphere(rng.random_float(), rng.random_float());
        const auto world_dir = local_to_world(dir_sample.dir);
        const float cos_out = dir_sample.dir.z;

        if (dir_sample.pdf <= 0.0f) {
            // pdf and cosine are zero! In theory impossible, but happens roughly once in a thousand frames in practice.
            // To prevent NaNs (cosine and pdf are divided by each other for the MIS weight), set values appropriately.
            // Numerical inaccuracies also cause this issue if the cosine is almost zero and the division by pi turns the pdf into zero
            sample.dir = world_dir;
            sample.radiance = float4(0.0f);
            sample.cos_out = 0.0f;
            sample.pdf_emit_w = 1.0f;
            sample.pdf_direct_a = 1.0f;
            return sample;
        }

        sample.dir      = world_dir;
        sample.radiance = intensity_ * area_ * pi; // The cosine cancels out with the pdf

        sample.cos_out      = cos_out;
        sample.pdf_emit_w   = dir_sample.pdf / area_;
        sample.pdf_direct_a = 1.0f / area_;

        return sample;
    }

    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) override {
        const float cos_theta_o = dot(normal_, out_dir);

        if (cos_theta_o <= 0.0f) {
            // set pdf to 1 to prevent NaNs
            pdf_direct_a = 1.0f;
            pdf_emit_w = 1.0f;
            return float4(0.0f);
        }

        pdf_direct_a = 1.0f / area_;
        auto local_out_dir = world_to_local(out_dir);
        pdf_emit_w   = 1.0f / area_ * cos_hemisphere_pdf(local_out_dir);

        return intensity_;
    }

    float3 world_to_local(const float3& dir) const {
        return float3(dot(binormal_, dir),
                      dot(tangent_, dir),
                      dot(normal_, dir));
    }

    float3 local_to_world(const float3& dir) const {
        return float3(binormal_.x * dir.x + tangent_.x * dir.y + normal_.x * dir.z,
                      binormal_.y * dir.x + tangent_.y * dir.y + normal_.y * dir.z,
                      binormal_.z * dir.x + tangent_.z * dir.y + normal_.z * dir.z);
    }

private:
    float3 p0_, p1_, p2_;
    float3 normal_;
    float3 tangent_;
    float3 binormal_;
    float4 intensity_;
    float area_;
};

class DirectionalLight : public Light {
public:
    // Keeps a reference to the bounding sphere of the scene, because the scene might change after the light is created.
    DirectionalLight(const float3& dir, const float4& intensity, BoundingSphere& scene_bounds)
        : dir_(dir), intensity_(intensity), scene_bounds_(scene_bounds)
    {
        local_coordinates(dir_, tangent_, binormal_);
    }

    virtual EmitSample sample_emit(RNG& rng) override {
        float2 disc_pos = sample_concentric_disc(rng.random_float(), rng.random_float());

        EmitSample sample;
        sample.pos = scene_bounds_.center + scene_bounds_.radius * (-dir_ + binormal_ * disc_pos.x + tangent_ * disc_pos.y);
        sample.dir = dir_;

        sample.pdf_direct_a = 1.0f;
        sample.pdf_emit_w   = concentric_disc_pdf() * scene_bounds_.inv_radius_sqr;
        sample.cos_out      = 1.0f;

        sample.radiance = intensity_ / sample.pdf_emit_w;

        return sample;
    }

    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        DirectIllumSample sample;

        sample.dir      = -dir_;
        sample.distance = FLT_MAX;
        sample.radiance = intensity_;

        sample.pdf_direct_w = 1.0f;
        sample.pdf_emit_w   = concentric_disc_pdf() * scene_bounds_.inv_radius_sqr;
        sample.cos_out      = 1.0f;

        return sample;
    }

    // You cannot randomly intersect a directional light.
    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) override { return float4(0.0f); }

    virtual bool is_delta() const override { return true; }
    virtual bool is_finite() const override { return false; }

private:
    float4 intensity_;
    float3 dir_;
    float3 tangent_, binormal_;
    BoundingSphere& scene_bounds_;
};

class PointLight : public Light {
public:
    PointLight(const float3& pos, const float4& intensity)
        : pos_(pos), intensity_(intensity)
    {}

    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) override {
        return float4(0.0f); // Point lights cannot be hit.
    }

    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        float3 dir = pos_ - from;
        const float sqdist = dot(dir, dir);
        const float dist   = sqrtf(sqdist);
        dir *= 1.0f / dist;

        DirectIllumSample sample;
        sample.dir       = dir;
        sample.distance  = dist,
        sample.radiance  = intensity_ / (4.0f * pi * sqdist);

        sample.pdf_direct_w = sqdist;
        sample.pdf_emit_w   = uniform_sphere_pdf();

        sample.cos_out = 1.0f; // Points do not have a normal.

        return sample;
    }

    virtual EmitSample sample_emit(RNG& rng) override {
        EmitSample sample;

        sample.pos      = pos_;
        sample.radiance = intensity_;

        auto dir_sample = sample_uniform_sphere(rng.random_float(), rng.random_float());
        sample.dir = dir_sample.dir;

        sample.pdf_direct_a = 1.0f;
        sample.pdf_emit_w   = dir_sample.pdf;

        sample.cos_out = 1.0f; // Points do not have a normal.

        return sample;
    }

    virtual bool is_delta() const override {
        return true;
    }

private:
    float4 intensity_;
    float3 pos_;
};

} // namespace imba

#endif // IMBA_LIGHT_H
