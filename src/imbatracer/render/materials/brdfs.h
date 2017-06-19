#ifndef IMBA_BRDFS_H
#define IMBA_BRDFS_H

#include "imbatracer/render/materials/bsdf.h"

namespace imba {

template <bool translucent = false>
class Lambertian : public BxDF {
public:
    Lambertian(const float3& n)
        : BxDF(n)
    {}

    rgb eval(const float3& out_dir, const float3& in_dir) const override final {
        float c = cos_theta(in_dir);
        if (!translucent)
            return cos_theta(out_dir) * c > 0 ? rgb(cos_theta(in_dir) / pi) : rgb(0.0f);
        else
            return cos_theta(out_dir) * c < 0 ? rgb(cos_theta(in_dir) / pi) : rgb(0.0f);
    }

    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const override final {
        DirectionSample ds = sample_cos_hemisphere(rng.random_float(), rng.random_float());
        in_dir = local_to_world(ds.dir);
        pdf = ds.pdf;

        if (translucent)
            in_dir = in_dir - 2.0f * normal;

        return rgb(1.0f); // cos/pi cancels out
    }
};

template <typename Fr>
class SpecularReflection : public BxDF {
public:
    SpecularReflection(const Fr& fresnel, const float3& n)
        : BxDF(n), fresnel_(fresnel)
    {}

    rgb eval(const float3& out_dir, const float3& in_dir) const override {
        return rgb(0.0f);
    }

    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const override {
        in_dir = reflect(out_dir);
        pdf = 1.0f;

        return rgb(fresnel_.eval(cos_theta(out_dir)));
    }

    float pdf(const float3& out_dir, const float3& in_dir) const override {
        return 0.0f; // Probability between any two randomly choosen directions is zero due to delta distribution.
    }

    float albedo(const float3& out_dir) const override {
        float fr = fresnel_.eval(cos_theta(out_dir));
        return fr;
    }

    bool specular() const override { return true; }

private:
    Fr fresnel_;
};

class Phong : public BxDF {
public:
    Phong(float exponent, const float3& n)
        : BxDF(n), exponent_(exponent)
    {}

    rgb eval(const float3& out_dir, const float3& in_dir) const override {
        auto reflected_in = reflect(in_dir);
        float cos_r_o = std::max(0.0f, dot(reflected_in, out_dir));
        cos_r_o = std::min(cos_r_o, 1.0f);

        float c = cos_theta(in_dir);
        return c * cos_theta(out_dir) > 0
               ? rgb((exponent_ + 2.0f) / (2.0f * pi) * powf(cos_r_o, exponent_) * c)
               : rgb(0.0f);
    }

    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const override {
        // Sample a power weighted direction relative to the reflected direction
        auto dir_sample = sample_power_cos_hemisphere(exponent_, rng.random_float(), rng.random_float());

        auto reflected_out = reflect(out_dir);
        float3 reflected_tan, reflected_binorm;
        local_coordinates(reflected_out, reflected_tan, reflected_binorm);

        auto& dir = dir_sample.dir;

        in_dir = float3(reflected_binorm.x * dir.x + reflected_tan.x * dir.y + reflected_out.x * dir.z,
                        reflected_binorm.y * dir.x + reflected_tan.y * dir.y + reflected_out.y * dir.z,
                        reflected_binorm.z * dir.x + reflected_tan.z * dir.y + reflected_out.z * dir.z);

        pdf = dir_sample.pdf;

        return same_hemisphere(out_dir, in_dir) ? eval(out_dir, in_dir) / pdf : rgb(0.0f);
    }

    float pdf(const float3& out_dir, const float3& in_dir) const override {
        return power_cos_hemisphere_pdf(exponent_, in_dir.z);
    }

private:
    float exponent_;
};


class OrenNayar : public BxDF {
public:
    OrenNayar(const rgb& reflectance, float roughness_degrees, const float3& n)
        : BxDF(n), reflectance_(reflectance)
    {
        param_sigma_ = radians(roughness_degrees);
        param_sigma_sqr_ = param_sigma_ * param_sigma_;
        param_a_ = 1.0f - param_sigma_sqr_ / (2.0f * (param_sigma_sqr_ + 0.33f));
        param_b_ = 0.45 * param_sigma_sqr_ / (param_sigma_sqr_ + 0.09f);
    }

    rgb eval(const float3& out_dir, const float3& in_dir) const override {
        float sin_theta_in = sin_theta(in_dir);
        float sin_theta_out = sin_theta(out_dir);

        float3 tangent, binormal;
        local_coordinates(normal, tangent, binormal);

        // Compute max(0, cos(phi_i - phi_o)) by using cos(a-b) = cos(a) cos(b) + sin(a) sin(b)
        float max_cos = 0.0f;
        if (sin_theta_in > 0.0001f && sin_theta_out > 0.0001f) {
            float sin_phi_in = sin_phi(in_dir, tangent, binormal);
            float cos_phi_in = cos_phi(in_dir, tangent, binormal);

            float sin_phi_out = sin_phi(out_dir, tangent, binormal);
            float cos_phi_out = cos_phi(out_dir, tangent, binormal);

            max_cos = std::max(0.0f, cos_phi_in * cos_phi_out + sin_phi_in * sin_phi_out);
        }

        float sin_alpha, tan_beta;
        if (abs_cos_theta(in_dir) > abs_cos_theta(out_dir)) {
            sin_alpha = sin_theta_out;
            tan_beta  = sin_theta_in / abs_cos_theta(in_dir);
        } else {
            sin_alpha = sin_theta_in;
            tan_beta  = sin_theta_out / abs_cos_theta(out_dir);
        }

        return same_hemisphere(out_dir, in_dir)
                ? reflectance_ * (1.0f / pi) * (param_a_ + param_b_ * max_cos * sin_alpha * tan_beta) * cos_theta(in_dir)
                : rgb(0.0f);
    }

private:
    rgb reflectance_;

    // Parameters for the reflection model.
    float param_sigma_;
    float param_sigma_sqr_;
    float param_a_;
    float param_b_;
};

/// Cook Torrance microfacet BRDF with Blinn distribution.
class CookTorrance : public BxDF {
public:
    CookTorrance(const rgb& reflectance, float eta, float kappa, float exponent, const float3& n)
        : BxDF(n),
          reflectance_(reflectance),
          fresnel_(eta, kappa),
          exponent_(exponent)
    {}

    rgb eval(const float3& out_dir, const float3& in_dir) const override {
        float c_out = cos_theta(out_dir);
        float c_in  = cos_theta(in_dir);
        if (fabsf(c_out) == 0.0f || fabsf(c_in) == 0.0f)
            return rgb(0.0f);

        auto half_dir = normalize(in_dir + out_dir);
        float cos_half = dot(in_dir, half_dir);

        auto fr = fresnel_.eval(cos_half);

        if (c_out * c_in < 0.0f)
            return rgb(0.0f);

        return (reflectance_ * blinn_distribution(half_dir) * geom_attenuation(out_dir, in_dir, half_dir) * fr)
                /
               (4.0f * fabsf(c_out));
    }

    rgb sample(const float3& out_dir, float3& in_dir, RNG& rng, float& pdf) const override {
        sample_blinn_distribution(out_dir, in_dir, rng.random_float(), rng.random_float(), pdf);
        return same_hemisphere(out_dir, in_dir) ? eval(out_dir, in_dir) / pdf : rgb(0.0f);
    }

    float pdf(const float3& out_dir, const float3& in_dir) const override {
        return same_hemisphere(out_dir, in_dir) ? blinn_distribution_pdf(out_dir, in_dir) : 0.0f;
    }

private:
    FresnelConductor fresnel_;
    rgb reflectance_;
    float exponent_;

    float geom_attenuation(const float3& out_dir, const float3& in_dir, const float3 half_dir) const {
        auto out_dot_half = dot(out_dir, half_dir);
        return std::min(1.0f, std::min(
            2.0f * abs_cos_theta(half_dir) * abs_cos_theta(out_dir) / out_dot_half,
            2.0f * abs_cos_theta(half_dir) * abs_cos_theta(in_dir)  / out_dot_half));
    }

    float blinn_distribution(const float3& half_dir) const {
        return (exponent_ + 2.0f) / (2.0f * pi) * powf(abs_cos_theta(half_dir), exponent_);
    }

    void sample_blinn_distribution(const float3& out_dir, float3& in_dir, float rnd_num_1, float rnd_num_2, float& pdf) const {
        // Compute the direction.
        float c_theta = powf(rnd_num_1, 1.0f / (exponent_ + 1.0f));
        float s_theta = sqrtf(std::max(0.0f, 1.0f - sqr(c_theta)));
        float phi = rnd_num_2 * 2.0f * pi;
        auto half_dir = spherical_dir(s_theta, c_theta, phi);

        // Flip if outgoing direction is on the opposite side of the face.
        if (!same_hemisphere(out_dir, half_dir)) half_dir = -half_dir;

        // Reflect outgoing direction about the half vector to obtain the incoming direction.
        in_dir = -out_dir + 2.0f * dot(out_dir, half_dir) * half_dir;

        if (dot(out_dir, half_dir) <= 0.0f)
            pdf = 1.0f; // Correct pdf would be zero. Because the value is zero in this case as well, that would generate a NaN, so we use one instead.
        else {
            pdf = (exponent_ + 1.0f) * powf(c_theta, exponent_) / (2.0f * pi * 4.0f * dot(out_dir, half_dir));
        }
    }

    float blinn_distribution_pdf(const float3& out_dir, const float3& in_dir) const {
        auto half_dir = normalize(in_dir + out_dir);

        if (dot(out_dir, half_dir) <= 0.0f)
            return 0.0f;
        else {
            return (exponent_ + 1.0f) * powf(abs_cos_theta(half_dir), exponent_) / (2.0f * pi * 4.0f * dot(out_dir, half_dir));
        }
    }
};

} // namespace imba

#endif
