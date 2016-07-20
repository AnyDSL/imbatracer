#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "random.h"
#include "../core/bsphere.h"

#include <cfloat>
#include <memory>

namespace imba {

/// Utility class to describe a surface that emits light.
struct AreaEmitter {
    AreaEmitter() {}
    AreaEmitter(const rgb& i, float a)
        : intensity(i)
        , area(a)
    {}

    /// Computes the amount of outgoing radiance from this emitter in a given direction
    rgb radiance(const float3& out_dir, const float3& normal, float& pdf_direct_a, float& pdf_emit_w) const {
        const float cos_theta_o = dot(normal, out_dir);

        if (cos_theta_o <= 0.0f) {
            // set pdf to 1 to prevent NaNs
            pdf_direct_a = 1.0f;
            pdf_emit_w = 1.0f;
            return rgb(0.0f);
        }

        pdf_direct_a = 1.0f / area;

        float3 tangent, binormal;
        local_coordinates(normal, tangent, binormal);

        auto local_out_dir = float3(dot(binormal, out_dir),
                                    dot(tangent, out_dir),
                                    dot(normal, out_dir));
        pdf_emit_w   = 1.0f / area * cos_hemisphere_pdf(local_out_dir);

        return intensity;
    }

    rgb intensity;
    float area;
};

class Light {
public:
    struct DirectIllumSample {
        float3 dir;
        float distance;

        rgb radiance;

        float cos_out;

        // Solid angle pdf for sampling this contribution during emission.
        float pdf_emit_w;
        // Solid angle pdf for sampling this contribution via direct illumination.
        float pdf_direct_w;
    };

    struct EmitSample {
        float3 pos;
        float3 dir;

        rgb radiance;

        float cos_out;

        // Solid angle pdf for sampling this contribution during emission.
        float pdf_emit_w;
        // Area pdf for sampling this contribution via direct illumination.
        float pdf_direct_a;
    };

    virtual ~Light() {}

    /// Samples an outgoing ray from the light source.
    virtual EmitSample sample_emit(RNG& rng) = 0;

    /// Samples a point on the light source. Used for shadow rays.
    virtual DirectIllumSample sample_direct(const float3& from, RNG& rng) = 0;

    /// Returns the area emitter associated with this light, or null if the light has no area.
    virtual const AreaEmitter* emitter() const { return nullptr; }

    virtual bool is_delta() const { return false; }
    virtual bool is_finite() const { return true; }
};

class TriangleLight : public Light {
public:
    TriangleLight(const rgb& intensity,
                  const float3& p0,
                  const float3& p1,
                  const float3& p2)
        : verts_{p0, p1, p2}
    {
        emit_.intensity = intensity;
        normal_ = cross(p1 - p0, p2 - p0);
        emit_.area = length(normal_) * 0.5f;
        normal_ = normalize(normal_);
        local_coordinates(normal_, tangent_, binormal_);
    }

    EmitSample sample_emit(RNG& rng) override {
        EmitSample sample;

        // Sample a point on the light source
        float u, v;
        sample_uniform_triangle(rng.random_float(), rng.random_float(), u, v);
        sample.pos = u * verts_[0] + v * verts_[1] + (1.0f - u - v) * verts_[2];

        // Sample an outgoing direction
        DirectionSample dir_sample = sample_cos_hemisphere(rng.random_float(), rng.random_float());
        const auto world_dir = dir_sample.dir.x * binormal_ +
                               dir_sample.dir.y * tangent_ +
                               dir_sample.dir.z * normal_;
        const float cos_out = dir_sample.dir.z;

        if (dir_sample.pdf <= 0.0f) {
            // pdf and cosine are zero! In theory impossible, but happens roughly once in a thousand frames in practice.
            // To prevent NaNs (cosine and pdf are divided by each other for the MIS weight), set values appropriately.
            // Numerical inaccuracies also cause this issue if the cosine is almost zero and the division by pi turns the pdf into zero
            sample.dir = world_dir;
            sample.radiance = rgb(0.0f);
            sample.cos_out = 0.0f;
            sample.pdf_emit_w = 1.0f;
            sample.pdf_direct_a = 1.0f;
            return sample;
        }

        sample.dir      = world_dir;
        sample.radiance = emit_.intensity * emit_.area * pi; // The cosine cancels out with the pdf

        sample.cos_out      = cos_out;
        sample.pdf_emit_w   = dir_sample.pdf / emit_.area;
        sample.pdf_direct_a = 1.0f / emit_.area;

        return sample;
    }

    DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        DirectIllumSample sample;

        // sample a point on the light source
        float u, v;
        sample_uniform_triangle(rng.random_float(), rng.random_float(), u, v);
        const float3 pos = u * verts_[0] + v * verts_[1] + (1.0f - u - v) * verts_[2];

        // compute distance and shadow ray direction
        sample.dir         = pos - from;
        const float distsq = dot(sample.dir, sample.dir);
        sample.distance    = sqrtf(distsq);
        sample.dir         = sample.dir * (1.0f / sample.distance);

        const float cos_out = dot(normal_, -1.0f * sample.dir);

        // directions form the opposite side of the light have zero intensity
        if (cos_out > 0.0f && cos_out < 1.0f) {
            sample.radiance = emit_.intensity * cos_out * (emit_.area / distsq);

            sample.cos_out      = cos_out;
            sample.pdf_emit_w   = (cos_out * 1.0f / pi) / emit_.area;
            sample.pdf_direct_w = 1.0f / emit_.area * distsq / cos_out;
        } else {
            sample.radiance = rgb(0.0f);

            // Prevent NaNs in the integrator
            sample.cos_out      = 1.0f;
            sample.pdf_emit_w   = 1.0f;
            sample.pdf_direct_w = 1.0f;
        }

        return sample;
    }

    const AreaEmitter* emitter() const override { return &emit_; }

    const float3& vertex(int i) { return verts_[i]; }

private:
    float3 verts_[3];
    float3 normal_;
    float3 tangent_;
    float3 binormal_;

    AreaEmitter emit_;
};

class DirectionalLight : public Light {
public:
    // Keeps a reference to the bounding sphere of the scene, because the scene might change after the light is created.
    DirectionalLight(const float3& dir, const rgb& intensity, const BSphere& bsphere)
        : dir_(dir), intensity_(intensity), bsphere_(bsphere)
    {
        local_coordinates(dir_, tangent_, binormal_);
    }

    EmitSample sample_emit(RNG& rng) override {
        float2 disc_pos = sample_concentric_disc(rng.random_float(), rng.random_float());

        EmitSample sample;
        sample.pos = bsphere_.center + bsphere_.radius * (-dir_ + binormal_ * disc_pos.x + tangent_ * disc_pos.y);
        sample.dir = dir_;

        sample.pdf_direct_a = 1.0f;
        sample.pdf_emit_w   = concentric_disc_pdf() * bsphere_.inv_radius_sqr;
        sample.cos_out      = 1.0f;

        sample.radiance = intensity_ / sample.pdf_emit_w;

        return sample;
    }

    DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        DirectIllumSample sample;

        sample.dir      = -dir_;
        sample.distance = FLT_MAX;
        sample.radiance = intensity_;

        sample.pdf_direct_w = 1.0f;
        sample.pdf_emit_w   = concentric_disc_pdf() * bsphere_.inv_radius_sqr;
        sample.cos_out      = 1.0f;

        return sample;
    }

    bool is_delta() const override { return true; }
    bool is_finite() const override { return false; }

private:
    rgb intensity_;
    float3 dir_;
    float3 tangent_, binormal_;
    BSphere bsphere_;
};

class PointLight : public Light {
public:
    PointLight(const float3& pos, const rgb& intensity)
        : pos_(pos), intensity_(intensity)
    {}

    EmitSample sample_emit(RNG& rng) override {
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

    DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
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

    bool is_delta() const override { return true; }

private:
    rgb intensity_;
    float3 pos_;
};

class SpotLight : public Light {
public:
    SpotLight(const float3& pos, const float3& dir, float angle, const rgb& intensity)
        : pos_(pos)
        , normal_(dir)
        , angle_(angle)
        , cos_angle_(cosf(angle))
        , intensity_(intensity)
    {
        local_coordinates(normal_, tangent_, binormal_);
    }

    EmitSample sample_emit(RNG& rng) override {
        EmitSample sample;

        sample.pos      = pos_;

        auto dir_sample = sample_uniform_cone(angle_, cos_angle_, rng.random_float(), rng.random_float());
        sample.dir = dir_sample.dir.z * normal_ + dir_sample.dir.x * tangent_ + dir_sample.dir.y * binormal_;

        sample.radiance = intensity_;

        sample.pdf_direct_a = 1.0f;
        sample.pdf_emit_w   = dir_sample.pdf;

        sample.cos_out = dir_sample.dir.z;

        return sample;
    }

    DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        float3 dir = pos_ - from;
        const float sqdist = dot(dir, dir);
        const float dist   = sqrtf(sqdist);
        dir *= 1.0f / dist;

        DirectIllumSample sample;
        sample.dir       = dir;
        sample.distance  = dist;
        sample.pdf_direct_w = sqdist;

        const float cos_o = -dot(dir, normal_);
        sample.radiance   = cos_o < cos_angle_ ? rgb(0.0f) : intensity_ / (4.0f * pi * sqdist);
        sample.pdf_emit_w = uniform_cone_pdf(angle_, cos_angle_, cos_o);
        sample.cos_out = cos_o;

        return sample;
    }

    bool is_delta() const override { return true; }

private:
    rgb intensity_;
    float3 pos_;
    float3 normal_;
    float3 binormal_;
    float3 tangent_;
    float angle_;
    float cos_angle_;
};

} // namespace imba

#endif // IMBA_LIGHT_H
