#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "../core/float3.h"
#include "material.h"
#include <cfloat>

namespace imba {

class Light {
public:
    struct LightSample {
        float3 dir;
        float distance;
        float4 intensity;

        float cos_out;
        float pdf_emission_w;
        float pdf_direct_w;
    };

    struct LightRaySample {
        float3 pos;
        float3 normal;
        float3 dir;

        float4 radiance;

        float cos_out;
        float pdf_emission_w;
        float pdf_direct_w;
    };

    // Samples an outgoing ray from the light source.
    virtual LightRaySample sample(RNG& rng) = 0;

    // Samples a point on the light source. Used for shadow rays.
    virtual LightSample sample(const float3& from, float u1, float u2) = 0;

    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) = 0;

    virtual ~Light() { }
};
/* TODO adapt to new system
class AreaLight : public Light {
public:
    AreaLight(const float3& point0, const float3& edge0, const float3& edge1, const float3& normal, const float4& intensity)
        : point0_(point0), edge0_(edge0), edge1_(edge1), area_(length(cross(edge0, edge1))), intensity_(intensity), normal_(normal)
    {}

    float area() { return area_; }

    // Samples a point on the light source. Used for shadow rays.
    virtual LightSample sample(const float3& from, float u1, float u2) override {
        LightSample s;
        // sample a point on the light source
        float3 pos = point0_ + u1 * edge0_ + u2 * edge1_;

        // compute distance and shadow ray direction
        s.dir = pos - from;
        float distsq = dot(s.dir, s.dir);
        s.distance = sqrtf(distsq);
        s.dir = s.dir * (1.0f / s.distance);

        float cos_normal_dir = dot(normal_, -1.0f * s.dir);

        // directions form the opposite side of the light have zero intensity
        if (cos_normal_dir > 0.0f && cos_normal_dir < 1.0f)
            s.intensity = intensity_ * cos_normal_dir * (area() / distsq);
        else
            s.intensity = float4(0.0f);

        return s;
    }

    virtual LightRaySample sample(RNG& rng) override {
        LightRaySample s;

        // sample a point and a direction on the light
        s.pos = point0_ + rng.random_float() * edge0_ + rng.random_float() * edge1_;
        DirectionSample ds = sample_hemisphere(normal_, rng.random_float(), rng.random_float());
        s.dir = ds.dir;
        s.intensity = intensity_ * (area() / ds.pdf);
        s.normal = normal_;

        return s;
    }

private:
    float3 point0_;
    float3 edge0_;
    float3 edge1_;
    float area_;
    float3 normal_;
    float4 intensity_;
};*/

class TriangleLight : public Light {
public:
    TriangleLight(float4 intensity, float3 p0, float3 p1, float3 p2) : intensity_(intensity), p0_(p0), p1_(p1), p2_(p2) {
        normal_ = cross(p1 - p0, p2 - p0);
        area_ = length(normal_) * 0.5f;
        normal_ = normalize(normal_);
    }

    inline float area() const { return area_; }

    // Samples a point on the light source. Used for shadow rays.
    virtual LightSample sample(const float3& from, float rnd1, float rnd2) override {
        LightSample s;

        // sample a point on the light source
        float u, v;
        uniform_sample_triangle(rnd1, rnd2, u, v);
        float3 pos = u * p0_ + v * p1_ + (1.0f - u - v) * p2_;

        // compute distance and shadow ray direction
        s.dir = pos - from;
        float distsq = dot(s.dir, s.dir);
        s.distance = sqrtf(distsq);
        s.dir = s.dir * (1.0f / s.distance);

        float cos_normal_dir = dot(normal_, -1.0f * s.dir);

        // directions form the opposite side of the light have zero intensity
        if (cos_normal_dir > 0.0f && cos_normal_dir < 1.0f)
            s.intensity = intensity_ * cos_normal_dir * (area_ / distsq);
        else
            s.intensity = float4(0.0f);

        s.cos_out = cos_normal_dir;
        s.pdf_emission_w = (cos_normal_dir * 1.0f / pi) / area_;
        s.pdf_direct_w = 1.0f / area_;

        return s;
    }

    virtual LightRaySample sample(RNG& rng) override {
        LightRaySample s;

        // Sample a point on the light source
        float u, v;
        uniform_sample_triangle(rng.random_float(), rng.random_float(), u, v);
        s.pos = u * p0_ + v * p1_ + (1.0f - u - v) * p2_;

        // Sample an outgoing direction
        DirectionSample ds = sample_cos_hemisphere(normal_, rng.random_float(), rng.random_float());
        float cos_out = dot(ds.dir, normal_);

        s.dir = ds.dir;
        s.radiance = intensity_ * cos_out * area_ / ds.pdf; // cosine cancels out with the pdf
        s.normal = normal_;

        s.cos_out = cos_out;
        s.pdf_emission_w = ds.pdf / area_;
        s.pdf_direct_w = 1.0f / area_;

        return s;
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

    bool inside_triangle(const float3& a, const float3& b, const float3& c, const float3& p) {
        // Compute vectors
        float3 v0 = c - a;
        float3 v1 = b - a;
        float3 v2 = p - a;

        // Compute dot products
        float dot00 = dot(v0, v0);
        float dot01 = dot(v0, v1);
        float dot02 = dot(v0, v2);
        float dot11 = dot(v1, v1);
        float dot12 = dot(v1, v2);

        // Compute barycentric coordinates
        float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
        float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
        float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

        // Check if point is in triangle
        return ((u >= 0.0f) && (v >= 0.0f) && (u + v < 1.0f));
    }
};

class DirectionalLight : public Light {
public:
    DirectionalLight(const float3& dir, const float4& intensity) : dir_(dir), intensity_(intensity) {}

    // Samples an outgoing ray from the light source.
    virtual LightRaySample sample(RNG& rng) override { return LightRaySample(); } // TODO

    // Samples a point on the light source. Used for shadow rays.
    virtual LightSample sample(const float3& from, float u1, float u2) override {
        LightSample sample = {
            -dir_,
            FLT_MAX,
            intensity_
        };
        // TODO pdf
        return sample;
    }

    // Only used for random rays intersection a light.
    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) { return float4(0.0f); }

private:
    float4 intensity_;
    float3 dir_;
};

class PointLight : public Light {
public:
    PointLight(const float3& pos, const float4& intensity) : pos_(pos), intensity_(intensity) {}

    // Samples an outgoing ray from the light source.
    virtual LightRaySample sample(RNG& rng) override { return LightRaySample(); } // TODO

    // Only used for random rays intersection a light.
    virtual float4 radiance(const float3& out_dir, float& pdf_direct_a, float& pdf_emit_w) { return float4(0.0f); }

    // Samples a point on the light source. Used for shadow rays.
    virtual LightSample sample(const float3& from, float u1, float u2) override {
        float3 dir = pos_ - from;
        const float sqdist = dot(dir, dir);
        const float dist = sqrtf(sqdist);
        dir *= 1.0f / dist;

        float4 intensity = intensity_ / (4.0f * pi * sqdist);

        LightSample sample = {
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