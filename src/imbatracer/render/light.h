#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "../core/float3.h"
#include "material.h"
#include <cfloat>

namespace imba {

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
        float3 normal;
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

    virtual ~Light() { }
};

class TriangleLight : public Light {
public:
    TriangleLight(float4 intensity, float3 p0, float3 p1, float3 p2) : intensity_(intensity), p0_(p0), p1_(p1), p2_(p2) {
        normal_ = cross(p1 - p0, p2 - p0);
        area_ = length(normal_) * 0.5f;
        normal_ = normalize(normal_);
    }

    inline float area() const { return area_; }

    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        DirectIllumSample sample;

        // sample a point on the light source
        float u, v;
        uniform_sample_triangle(rng.random_float(), rng.random_float(), u, v);
        const float3 pos = u * p0_ + v * p1_ + (1.0f - u - v) * p2_;

        // compute distance and shadow ray direction
        sample.dir = pos - from;
        const float distsq = dot(sample.dir, sample.dir);
        sample.distance = sqrtf(distsq);
        sample.dir = sample.dir * (1.0f / sample.distance);

        const float cos_out = dot(normal_, -1.0f * sample.dir);

        // directions form the opposite side of the light have zero intensity
        if (cos_out > 0.0f && cos_out < 1.0f)
            sample.radiance = intensity_ * cos_out * (area_ / distsq);
        else
            sample.radiance = float4(0.0f);

        sample.cos_out = cos_out;
        sample.pdf_emit_w = (cos_out * 1.0f / pi) / area_;
        sample.pdf_direct_w = 1.0f / area_ * distsq / cos_out;

        return sample;
    }

    virtual EmitSample sample_emit(RNG& rng) override {
        EmitSample sample;

        // Sample a point on the light source
        float u, v;
        uniform_sample_triangle(rng.random_float(), rng.random_float(), u, v);
        sample.pos = u * p0_ + v * p1_ + (1.0f - u - v) * p2_;

        // Sample an outgoing direction
        DirectionSample dir_sample = sample_cos_hemisphere(normal_, rng.random_float(), rng.random_float());
        float cos_out = dot(dir_sample.dir, normal_);

        sample.dir = dir_sample.dir;
        sample.radiance = intensity_ * area_ * pi; // The cosine cancels out with the pdf
        sample.normal = normal_;

        sample.cos_out = cos_out;
        sample.pdf_emit_w = dir_sample.pdf / area_;
        sample.pdf_direct_a = 1.0f / area_;

        return sample;
    }

    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) override {
        const float cos_theta_o = dot(normal_, out_dir);

        if (cos_theta_o <= 0.0f)
            return float4(0.0f);

        pdf_direct_a = 1.0f / area_;
        pdf_emit_w = 1.0f / area_ * cos_hemisphere_pdf(normal_, out_dir);

        return intensity_;
    }

private:
    float3 p0_, p1_, p2_;
    float3 normal_;
    float4 intensity_;
    float area_;
};

class DirectionalLight : public Light {
public:
    DirectionalLight(const float3& dir, const float4& intensity) : dir_(dir), intensity_(intensity) {}

    // Samples an outgoing ray from the light source.
    virtual EmitSample sample_emit(RNG& rng) override { return EmitSample(); } // TODO

    // Samples a point on the light source. Used for shadow rays.
    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        DirectIllumSample sample = {
            -dir_,
            FLT_MAX,
            intensity_
        };
        // TODO pdf
        return sample;
    }

    // Only used for random rays intersection a light.
    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) override { return float4(0.0f); }

private:
    float4 intensity_;
    float3 dir_;
};

class PointLight : public Light {
public:
    PointLight(const float3& pos, const float4& intensity) : pos_(pos), intensity_(intensity) {}

    // Samples an outgoing ray from the light source.
    virtual EmitSample sample_emit(RNG& rng) override { return EmitSample(); } // TODO

    // Only used for random rays intersection a light.
    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) override { return float4(0.0f); }

    // Samples a point on the light source. Used for shadow rays.
    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        float3 dir = pos_ - from;
        const float sqdist = dot(dir, dir);
        const float dist = sqrtf(sqdist);
        dir *= 1.0f / dist;

        float4 intensity = intensity_ / (4.0f * pi * sqdist);

        DirectIllumSample sample = {
            dir,
            dist,
            intensity
        };
        // TODO pdf
        return sample;
    }

private:
    float4 intensity_;
    float3 pos_;
};

using LightContainer = std::vector<std::unique_ptr<Light>>;

} // namespace imba

#endif
