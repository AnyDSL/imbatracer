#ifndef IMBA_MATERIAL_H
#define IMBA_MATERIAL_H

#include "random.h"
#include "texture_sampler.h"
#include "light.h"
#include "intersection.h"

#include "../core/float3.h"
#include "../core/common.h"

#include <iostream>

namespace imba {

struct Material {
    enum Kind {
        lambert,
        mirror,
        emissive,
        combine,
        glass
    } kind;

    // whether or not the material is described by a delta distribution
    bool is_delta;

    Material(Kind k, bool is_delta) : kind(k), is_delta(is_delta) {}
    virtual ~Material() {}
};

float4 evaluate_material(Material* mat, const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev);
float4 sample_material(Material* mat, const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular);
float pdf_material(Material* mat, const Intersection& isect, const float3& in_dir);

inline float fresnel_conductor(float cosi, float eta, float kappa)
{
    const float ekc = (eta*eta + kappa*kappa) * cosi*cosi;
    const float par =
        (ekc - (2.f * eta * cosi) + 1) /
        (ekc + (2.f * eta * cosi) + 1);

    const float ek = eta*eta + kappa*kappa;
    const float perp =
        (ek - (2.f * eta * cosi) + cosi*cosi) /
        (ek + (2.f * eta * cosi) + cosi*cosi);

    return (par + perp) / 2.f;
}

inline float fresnel_dielectric(float cosi, float coso, float etai, float etao)
{
    const float par  = (etao * cosi - etai * coso) / (etao * cosi + etai * coso);
    const float perp = (etai * cosi - etao * coso) / (etai * cosi + etao * coso);

    return (par * par + perp * perp) / 2.f;
}

class LambertMaterial : public Material {
public:
    LambertMaterial() : Material(lambert, false), diffuse_(1.0f, 0.0f, 1.0f, 1.0f), sampler_(nullptr) { }
    LambertMaterial(const float4& color) : Material(lambert, false), diffuse_(color), sampler_(nullptr) { }
    LambertMaterial(TextureSampler* sampler) : Material(lambert, false), sampler_(sampler) { }

    inline float4 sample(const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(isect.uv);
        }

        auto normal = isect.normal;
        auto geom_normal = isect.geom_normal;
        // Flip the normal to lie on the same side as the ray direction.
        if (dot(isect.out_dir, normal) < 0.0f) normal *= -1.0f;
        // Flip geometric normal to same side as the shading normal.
        if (dot(normal, geom_normal) < 0.0f) geom_normal *= -1.0f;

        DirectionSample hemi_sample = sample_cos_hemisphere(normal, rng.random_float(), rng.random_float());
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        specular = false;

        if (dot(in_dir, geom_normal) * dot(isect.out_dir, geom_normal) <= 0.0f)
            return float4(0.0f);

        return clr; // Cosine and 1/pi cancle out with pdf from the hemisphere sampling.
    }

    inline float4 eval(const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(isect.uv);
        }

        if (dot(in_dir, isect.geom_normal) * dot(isect.out_dir, isect.geom_normal) <= 0.0f)
            return float4(0.0f);

        pdf_dir = (1.0f / pi) * std::max(0.0f, dot(isect.normal, in_dir));
        pdf_rev = (1.0f / pi) * std::max(0.0f, dot(isect.normal, isect.out_dir));

        if (adjoint)
            return clr * (1.0f / pi) * fabsf(dot(isect.normal, isect.out_dir)) * (dot(isect.geom_normal, in_dir) / dot(isect.geom_normal, isect.out_dir));
        else
            return clr * (1.0f / pi) * fabsf(dot(isect.normal, in_dir));
    }

    inline float pdf(const Intersection& isect, const float3& in_dir) {
        return (1.0f / pi) * fabsf(dot(isect.normal, in_dir));
    }

private:
    float4 diffuse_;
    TextureSampler* sampler_;
};

/// Combines two materials together using weights from a texture. 0 => full contribution from the first material
/// 1 => full contribution from the second material
class CombineMaterial : public Material {
public:
    CombineMaterial(TextureSampler* scale, std::unique_ptr<Material> m1, std::unique_ptr<Material> m2)
        : Material(combine, m1->is_delta && m2->is_delta), scale_(scale), m1_(std::move(m1)), m2_(std::move(m2)) { }

    inline float4 sample(const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular) {
        const float s = scale_->sample(isect.uv).x;
        const float r = rng.random_float();
        if (r < s) {
            return sample_material(m1_.get(), isect, rng, in_dir, adjoint, pdf, specular);
        } else {
            return sample_material(m2_.get(), isect, rng, in_dir, adjoint, pdf, specular);
        }
    }

    inline float4 eval(const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev) {
        const float s = scale_->sample(isect.uv).x;

        const float4 v1 = evaluate_material(m1_.get(), isect, in_dir, adjoint, pdf_dir, pdf_rev);

        float pd, pr;
        const float4 v2 = evaluate_material(m2_.get(), isect, in_dir, adjoint, pd, pr);

        pdf_dir *= pd;
        pdf_rev *= pr;

        return v1 * s + (1.0f - s)  * v2;
    }

    inline float pdf(const Intersection& isect, const float3& in_dir) {
        return 0.0f; // TODO
    }

private:
    TextureSampler* scale_;
    std::unique_ptr<Material> m1_;
    std::unique_ptr<Material> m2_;
};


/// Perfect mirror reflection.
class MirrorMaterial : public Material {
public:
    MirrorMaterial(float eta, float kappa, const float3& ks) : Material(mirror, true), eta_(eta), kappa_(kappa), ks_(ks, 1.0f) { }

    inline float4 sample(const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular) {
        // calculate the reflected direction
        in_dir = -isect.out_dir + 2.0f * isect.normal * dot(isect.out_dir, isect.normal);
        float cos_theta = fabsf(dot(isect.normal, isect.out_dir));

        pdf = 1.0f;
        specular = true;

        return float4(fresnel_conductor(cos_theta, eta_, kappa_));
    }

    inline float4 eval(const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev) {
        pdf_rev = pdf_dir = 0.0f;
        return float4(0.0f);
    }

    inline float pdf(const Intersection& isect, const float3& in_dir) {
        return 0.0f;
    }

private:
    float eta_;
    float kappa_;
    float4 ks_;
};

class GlassMaterial : public Material {
public:
    GlassMaterial(float eta, const float3& tf, const float3& ks) : Material(glass, true), eta_(eta), tf_(tf, 1.0f), ks_(/*ks,*/ 1.0f) {}

    inline float4 sample(const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular) {
        specular = true;

        float3 normal = isect.normal;

        float cos_theta = dot(normal, isect.out_dir);
        float eta_i = 1.0f;
        float eta_o = eta_;

        if (cos_theta < 0) {
            std::swap(eta_i, eta_o);
            cos_theta = -cos_theta;
            normal = -normal;
        }

        const float etafrac = eta_i / eta_o;
        const float sin2sq = etafrac * etafrac * (1.0f - cos_theta * cos_theta);

        const float3 reflect_dir = reflect(-isect.out_dir, normal);

        if (sin2sq >= 1.0f) {
            // total internal reflection
            in_dir = reflect_dir;
            pdf = 1.0f;
            return float4(1.0f);
        }

        const float cos_o = sqrtf(1.0f - sin2sq);
        const float fr = fresnel_dielectric(cos_theta, cos_o, eta_i, eta_o);

        const float rnd_num = rng.random_float();
        if (rnd_num < fr) {
            in_dir = reflect_dir;
            pdf = fr;
            return ks_;
        } else {
            const float3 refract_dir = -etafrac * isect.out_dir + (etafrac * cos_theta - cos_o) * normal;

            in_dir = refract_dir;
            pdf = 1.0f / fr;

            if (adjoint)
                return tf_;
            else
                return tf_ * (1.0f / (etafrac * etafrac));
        }
    }

    inline float4 eval(const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev) {
        pdf_rev = pdf_dir = 0.0f;
        return float4(0.0f);
    }

    inline float pdf(const Intersection& isect, const float3& in_dir) {
        return 0.0f;
    }

private:
    float eta_;
    float4 tf_;
    float4 ks_;
};

/// Material for diffuse emissive objects.
class EmissiveMaterial : public Material {
public:
    EmissiveMaterial(const float4& color) : color_(color), Material(emissive, true) { }

    inline float4 sample(const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular) {
        /*// uniform sample the hemisphere
        DirectionSample hemi_sample = sample_cos_hemisphere(isect.normal, rng.random_float(), rng.random_float());
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        specular = false;
        return float4(0.0f) * (1.0f / pi);*/
        pdf = 0.0f;
        specular = true;
        return float4(0.0f);
    }

    inline float4 eval(const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev) {
        pdf_rev = pdf_dir = 1.0f;
        return float4(0.0f);
    }

    inline float pdf(const Intersection& isect, const float3& in_dir) {
        return 0.0f;
    }

    inline float4 color() { return color_; }

    inline void set_light(Light* l) { light_ = l; }
    inline Light* light() { return light_; }

private:
    float4 color_;
    Light* light_;
};

#define ALL_MATERIALS() \
    HANDLE_MATERIAL(Material::lambert, LambertMaterial) \
    HANDLE_MATERIAL(Material::mirror, MirrorMaterial) \
    HANDLE_MATERIAL(Material::emissive, EmissiveMaterial) \
    HANDLE_MATERIAL(Material::combine, CombineMaterial) \
    HANDLE_MATERIAL(Material::glass, GlassMaterial)

inline float4 sample_material(Material* mat, const Intersection& isect, RNG& rng, float3& in_dir, bool adjoint, float& pdf, bool& specular) {
    switch (mat->kind) {
#define HANDLE_MATERIAL(m,T) \
        case m: return static_cast<T*>(mat)->sample(isect, rng, in_dir, adjoint, pdf, specular);
    ALL_MATERIALS()
#undef HANDLE_MATERIAL
    }
}

inline float4 evaluate_material(Material* mat, const Intersection& isect, const float3& in_dir, bool adjoint, float& pdf_dir, float& pdf_rev) {
    switch (mat->kind) {
#define HANDLE_MATERIAL(m,T) \
        case m: return static_cast<T*>(mat)->eval(isect, in_dir, adjoint, pdf_dir, pdf_rev);
    ALL_MATERIALS()
#undef HANDLE_MATERIAL
    }
}

inline float pdf_material(Material* mat, const Intersection& isect, const float3& in_dir) {
    switch (mat->kind) {
#define HANDLE_MATERIAL(m,T) \
        case m: return static_cast<T*>(mat)->pdf(isect, in_dir);
    ALL_MATERIALS()
#undef HANDLE_MATERIAL
    }
}

#undef ALL_MATERIALS

using MaterialContainer = std::vector<std::unique_ptr<imba::Material>>;

}

#endif
