#ifndef IMBA_MATERIAL_H
#define IMBA_MATERIAL_H

#include "../core/float3.h"
#include "../core/common.h"
#include "random.h"
#include "texture_sampler.h"

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
    
    Material(Kind k) : kind(k) { }
    virtual ~Material() { }
};

struct SurfaceInfo {
    float3 normal;
    float2 uv;
    float3 geom_normal;
};

float4 evaluate_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir);
float4 sample_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular);

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
    LambertMaterial() : Material(lambert), diffuse_(1.0f, 0.0f, 1.0f, 1.0f), sampler_(nullptr) { }
    LambertMaterial(const float4& color) : Material(lambert), diffuse_(color), sampler_(nullptr) { }
    LambertMaterial(TextureSampler* sampler) : Material(lambert), sampler_(sampler) { }  
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(surf.uv);
        }
        
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(surf.normal, rng.random_float(), rng.random_float());
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        specular = false;
        return clr * (1.0f / pi);
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(surf.uv);
        }

        return clr * (1.0f / pi); 
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
        : Material(combine), scale_(scale), m1_(std::move(m1)), m2_(std::move(m2)) { }
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        const float s = scale_->sample(surf.uv).x;
        const float r = rng.random_float();
        if (r < s) {
            return sample_material(m1_.get(), out_dir, surf, rng, in_dir, pdf, specular);
        } else {
            return sample_material(m2_.get(), out_dir, surf, rng, in_dir, pdf, specular);
        }
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        const float s = scale_->sample(surf.uv).x;
        const float4 v1 = evaluate_material(m1_.get(), out_dir, surf, in_dir);
        const float4 v2 = evaluate_material(m2_.get(), out_dir, surf, in_dir);
        return v1 * s + (1.0f - s)  * v2; 
    }
    
private:
    TextureSampler* scale_;
    std::unique_ptr<Material> m1_;
    std::unique_ptr<Material> m2_;
};


/// Perfect mirror reflection.
class MirrorMaterial : public Material {
public:
    MirrorMaterial(float eta, float kappa, const float3& ks) : Material(mirror), eta_(eta), kappa_(kappa), ks_(ks, 1.0f) { }

    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        // calculate the reflected direction
        in_dir = -out_dir + 2.0f * surf.normal * dot(out_dir, surf.normal);
        float cos_theta = fabsf(dot(surf.normal, out_dir));

        pdf = 1.0f;
        specular = true;

        return float4(fresnel_conductor(cos_theta, eta_, kappa_) / cos_theta);
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return float4(0.0f);
    }

private:
    float eta_;
    float kappa_;
    float4 ks_;
};

class GlassMaterial : public Material {
public:
    GlassMaterial(float eta, const float3& tf, const float3& ks) : Material(glass), eta_(eta), tf_(tf, 1.0f), ks_(ks, 1.0f) {}

    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        specular = true;

        float cos_theta = dot(surf.normal, out_dir);
        cos_theta = clamp(cos_theta, -1.0f, 1.0f);

        bool entering = cos_theta > 0;
        float eta_i = 1.0f;
        float eta_o = eta_;

        if (!entering) {
            std::swap(eta_i, eta_o);
            cos_theta = -cos_theta;
        }

        float etafrac = eta_i / eta_o;
        float sin2sq = etafrac * etafrac * (1.0f - cos_theta*cos_theta);

        float3 reflect_dir = -out_dir + 2.0f * (surf.normal * dot(out_dir, surf.normal));
        reflect_dir = normalize(reflect_dir);

        if (sin2sq >= 1.0f) {
            // total internal reflection
            in_dir = reflect_dir;
            pdf = 1.0f;
            return float4(0.0f);
        }

        float cos_o = sqrtf(std::max(0.0f, 1.f - sin2sq));
        float fr = fresnel_dielectric(cos_theta, cos_o, eta_i, eta_o);

        float rnd_num = rng.random_float();
        if (rnd_num < fr) {
            in_dir = reflect_dir;
            pdf = 1.0f;
            return float4(ks_ / fabsf(cos_theta));
        } else {
            float3 refract_dir = etafrac * (-out_dir) +
                (etafrac * cos_theta - cos_o) * surf.normal;

            in_dir = refract_dir;
            pdf = 1.0f;

            return float4(((eta_o * eta_o) / (eta_i * eta_i) * tf_) / fabsf(cos_theta));
        }
    }

    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return float4(0.0f);
    }

private:
    float eta_;
    float4 tf_;
    float4 ks_;
};

/// Material for diffuse emissive objects.
class EmissiveMaterial : public Material {
public:
    EmissiveMaterial(const float4& color) : color_(color), Material(emissive) { }
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(surf.normal, rng.random_float(), rng.random_float());
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        specular = false;
        return color_ * (1.0f / pi);
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return color_ * (1.0f / pi);
    }
    
    inline float4 color() { return color_; }
    
private:
    float4 color_;
};

#define ALL_MATERIALS() \
    HANDLE_MATERIAL(Material::lambert, LambertMaterial) \
    HANDLE_MATERIAL(Material::mirror, MirrorMaterial) \
    HANDLE_MATERIAL(Material::emissive, EmissiveMaterial) \
    HANDLE_MATERIAL(Material::combine, CombineMaterial) \
    HANDLE_MATERIAL(Material::glass, GlassMaterial)

inline float4 sample_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
    switch (mat->kind) {
#define HANDLE_MATERIAL(m,T) \
        case m: return static_cast<T*>(mat)->sample(out_dir, surf, rng, in_dir, pdf, specular);
    ALL_MATERIALS()
#undef HANDLE_MATERIAL
    }
}

inline float4 evaluate_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
    switch (mat->kind) {
#define HANDLE_MATERIAL(m,T) \
        case m: return static_cast<T*>(mat)->eval(out_dir, surf, in_dir);
    ALL_MATERIALS()
#undef HANDLE_MATERIAL
    }
}

using MaterialContainer = std::vector<std::unique_ptr<imba::Material>>;

}

#endif
