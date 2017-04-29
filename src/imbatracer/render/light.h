#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "random.h"
#include "../core/bsphere.h"
#include "../core/image.h"

#include <cfloat>
#include <memory>

namespace imba {

/// Utility class to describe a triangular surface that emits light.
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
        pdf_emit_w   = 1.0f / area * cos_hemisphere_pdf(local_out_dir.z);

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
        sample.dir = dir_sample.dir.x * binormal_ +
                     dir_sample.dir.y * tangent_ +
                     dir_sample.dir.z * normal_;
        const float cos_out = dir_sample.dir.z;

        if (dir_sample.pdf <= 0.0f) {
            // pdf and cosine are zero! In theory impossible, but happens roughly once in a thousand frames in practice.
            // To prevent NaNs (cosine and pdf are divided by each other for the MIS weight), set values appropriately.
            // Numerical inaccuracies also cause this issue if the cosine is almost zero and the division by pi turns the pdf into zero
            sample.radiance = rgb(0.0f);
            sample.cos_out = 0.0f;
            sample.pdf_emit_w = 1.0f;
            sample.pdf_direct_a = 1.0f;
            return sample;
        }

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
            sample.pdf_emit_w   = cos_hemisphere_pdf(cos_out) / emit_.area;
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
        : dir_(dir), intensity_(intensity), bsphere_(&bsphere)
    {
        local_coordinates(dir_, tangent_, binormal_);
    }

    EmitSample sample_emit(RNG& rng) override {
        float2 disc_pos = sample_concentric_disc(rng.random_float(), rng.random_float());

        EmitSample sample;
        sample.pos = bsphere_->center + bsphere_->radius * (-dir_ + binormal_ * disc_pos.x + tangent_ * disc_pos.y);
        sample.dir = dir_;

        sample.pdf_direct_a = 1.0f;
        sample.pdf_emit_w   = concentric_disc_pdf() * bsphere_->inv_radius_sqr;
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
        sample.pdf_emit_w   = concentric_disc_pdf() * bsphere_->inv_radius_sqr;
        sample.cos_out      = 1.0f;

        return sample;
    }

    bool is_delta() const override { return true; }
    bool is_finite() const override { return false; }

private:
    rgb intensity_;
    float3 dir_;
    float3 tangent_, binormal_;
    // The scene geometry is not yet known when the lights are created.
    // Hence we store a pointer to the bounding sphere which will be updated later on.
    const BSphere* bsphere_;
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

        auto dir_sample = sample_uniform_cone(cos_angle_, rng.random_float(), rng.random_float());
        sample.cos_out = 1.0f;
        sample.dir = dir_sample.dir.x * binormal_ +
                     dir_sample.dir.y * tangent_ +
                     dir_sample.dir.z * normal_;

        sample.radiance = intensity_ / dir_sample.pdf;

        sample.pdf_direct_a = 1.0f;
        sample.pdf_emit_w   = dir_sample.pdf;

        return sample;
    }

    DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        float3 dir = pos_ - from;
        const float sqdist = dot(dir, dir);
        const float dist   = sqrtf(sqdist);
        dir *= 1.0f / dist;

        const float cos_o = -dot(dir, normal_);

        DirectIllumSample sample;
        sample.dir       = dir;
        sample.distance  = dist;
        sample.pdf_direct_w = sqdist;

        if (cos_o < cos_angle_) {
            sample.radiance   = rgb(0.0f);
            sample.pdf_emit_w = 0.0f;
        } else {
            sample.radiance   = intensity_ / sqdist;
            sample.pdf_emit_w = uniform_cone_pdf(cos_angle_, cos_o);
        }

        sample.cos_out = 1.0f;

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

class EnvMap {
public:
    EnvMap(const Image& img, float intensity, const BSphere& bsphere)
    : img_(img), intensity_(intensity), bsphere_(bsphere)
    {
        // Compute the piecewise constant pdf, marginal pdf, and corresponding cdfs for sampling.
        const int w = img.width();
        const int h = img.height();

        float total_value = 0.0f;

        // Compute piecewise constant pdf, cdf, and marginals.
        func_.resize(w * h);
        marginal_.resize(h);
        cdf_.resize((w + 1) * h);
        for (int row = 0; row < h; ++row) {
            marginal_[row] = 0.0f;
            for (int col = 0; col < w; ++col) {
                // Average over four pixels (with repeat boundary handling)
                func_[row * w + col] = luminance(img_(row,           col          )) +
                                       luminance(img_(row,           (col + 1) % w)) +
                                       luminance(img_((row + 1) % h, col          )) +
                                       luminance(img_((row + 1) % h, (col + 1) % w));
                func_[row * w + col] *= 0.25f;

                const float sin_theta = 1.0f;
                func_[row * w + col] *= sin_theta;

                total_value += func_[row * w + col];

                marginal_[row] += func_[row * w + col];
            }

            // Sum up values to compute cdf for this row
            cdf_[row * (w + 1)] = 0.0f;
            for (int col = 0; col < w; ++col)
                cdf_[row * (w + 1) + col + 1] = cdf_[row * (w + 1) + col] + func_[row * w + col] / w;
            for (int col = 0; col <= w; ++col)
                cdf_[row * (w + 1) + col] /= cdf_[row * (w + 1) + w + 1];
        }

        // Compute marginal CDF for sampling
        marginal_cdf_.resize(h + 1);
        marginal_cdf_[0] = 0.0f;
        for (int row = 0; row < h; ++row)
            marginal_cdf_[row + 1] = marginal_cdf_[row] + marginal_[row] / h;
        for (int row = 0; row <= h; ++row)
            marginal_cdf_[row] /= marginal_cdf_.back();

        // Compute the actual pdf values.
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                func_[row * w + col] /= total_value / (w * h);
            }
        }
    }

    /// Returns the amount of radiance due to environment map lighting from a certain direction.
    rgb radiance(const float3& out_dir, float& pdf_direct_w, float& pdf_emit_w) const {
        // Compute lat/long coordinates in the image.
        float phi = atan2f(out_dir.x, out_dir.z);
        phi = (phi < 0.0f) ? (phi + 2.0f * pi) : phi;

        const float s = phi / (2.0f * pi);
        const float t = acosf(out_dir.y) / pi;

        pdf_direct_w = pdf(s, t) / (2.0f * pi * pi * sinf(t));
        pdf_emit_w   = concentric_disc_pdf() * bsphere_.inv_radius_sqr * pdf_direct_w;

        return intensity_ * static_cast<rgb>(img_(s * (img_.width() - 1), t * (img_.height() - 1)));
    }

    float pdf(float s, float t) const {
        return 1.0f;// / float(img_.width() * img_.height());
    }

    /// Samples a direction for incoming light from the environment map, using importance sampling.
    float3 sample_dir(RNG& rng, float3& dir, float& pdf) const {
        float2 uv;
        const auto color = sample_uv(rng, uv, pdf);

        // Convert uv point to spherical coordinates and compute direction out of those.
        const float theta = pi * uv.y;
        const float sin_theta = sinf(theta);
        const float phi = 2.0f * pi * uv.x;
        dir = float3(sin_theta * sinf(phi),
                     cosf(theta),
                     sin_theta * cosf(phi));

        if (sin_theta != 0.0f)
            // Transform pdf from image space sampling to solid angle.
            pdf /= 2.0f * pi * pi * sin_theta;
        else
            pdf = 0.0f;

        return color;
    }

    /// Importance samples a point on the environment map.
    rgb sample_uv(RNG& rng, float2& uv, float& pdf) const {
        pdf = 1.0f / float(img_.width() * img_.height());

        uv.x = rng.random_float();
        uv.y = rng.random_float();

        // TODO importance sample the image.

        // Transform pdf for transformation from pixel coordinates to uv
        pdf *= float(img_.width() * img_.height());

        return intensity_ * static_cast<rgb>(img_(uv.x * (img_.width() - 1), uv.y * (img_.height() - 1)));
    }

private:
    Image img_;
    float intensity_;

    std::vector<float> func_;
    std::vector<float> cdf_;
    std::vector<float> marginal_;
    std::vector<float> marginal_cdf_;

    // The scene geometry is not yet known when the lights are created. Hence we store a reference to the bounding sphere which will be updated later on.
    const BSphere& bsphere_;
};

class EnvLight : public Light {
public:
    EnvLight(const EnvMap* map, const BSphere& bsphere)
        : bsphere_(bsphere), map_(map)
    {}

    /// Samples an outgoing ray from the light source.
    EmitSample sample_emit(RNG& rng) override {
        float pdf;
        float3 dir;
        auto radiance = map_->sample_dir(rng, dir, pdf);
        dir *= -1.0f;

        float2 disc_pos = sample_concentric_disc(rng.random_float(), rng.random_float());

        float3 tangent, binormal;
        local_coordinates(dir, tangent, binormal);

        EmitSample sample;
        sample.pos = bsphere_.center + bsphere_.radius * (-dir + binormal * disc_pos.x + tangent * disc_pos.y);
        sample.dir = dir;

        sample.pdf_direct_a = pdf;
        sample.pdf_emit_w   = concentric_disc_pdf() * bsphere_.inv_radius_sqr * pdf;
        sample.cos_out      = 1.0f;

        sample.radiance = radiance / sample.pdf_emit_w;

        return sample;
    }

    /// Samples a point on the light source. Used for shadow rays.
    DirectIllumSample sample_direct(const float3& from, RNG& rng) override {
        float pdf;
        float3 dir;
        auto radiance = map_->sample_dir(rng, dir, pdf);

        DirectIllumSample sample;

        sample.dir      = dir;
        sample.distance = FLT_MAX;
        sample.radiance = radiance / pdf;

        sample.pdf_direct_w = pdf;
        sample.pdf_emit_w   = concentric_disc_pdf() * bsphere_.inv_radius_sqr * pdf;
        sample.cos_out      = 1.0f;

        return sample;
    }

    bool is_delta()  const override { return false; }
    bool is_finite() const override { return false; }

private:
    const EnvMap* map_;
    // The scene geometry is not yet known when the lights are created. Hence we store a reference to the bounding sphere which will be updated later on.
    const BSphere& bsphere_;
};

} // namespace imba

#endif // IMBA_LIGHT_H
