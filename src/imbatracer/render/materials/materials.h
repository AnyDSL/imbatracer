#ifndef IMBA_MATERIALS_H
#define IMBA_MATERIALS_H

#include "brdfs.h"
#include "btdfs.h"

#include "../light.h"
#include "../texture_sampler.h"

namespace imba {

class Material {
public:
    Material(TextureSampler* bump) : light_(nullptr), bump_(bump) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint=false) const = 0;

    /// Associates the material with a light source.
    inline void set_light(Light* l) { light_ = l; }

    /// If the material is attached to a light source, this returns the lightsource, otherwise nullptr.
    inline Light* light() { return light_; }

    virtual bool is_specular() { return false; }

    /// Updates the shading normal of the given intersection using bump mapping.
    inline void bump(Intersection& isect) {
        if (!bump_)
            return;

        auto u_displace = bump_->sample_du(isect.uv);
        auto v_displace = bump_->sample_dv(isect.uv);
        auto displace   = bump_->sample(isect.uv);

        auto diff_u = (u_displace - displace).x / (bump_->du() * 10.0f);
        auto diff_v = (v_displace - displace).x / (bump_->dv() * 10.0f);

        auto n = cross(isect.v_tangent, isect.u_tangent);
        auto u = isect.u_tangent + diff_u * n;
        auto v = isect.v_tangent + diff_v * n;

        isect.normal = normalize(cross(u, v));
    }

private:
    Light* light_;
    TextureSampler* bump_;
};

using MaterialContainer = std::vector<std::unique_ptr<Material>>;

/// Very simple material with a lambertian BRDF.
class DiffuseMaterial : public Material {
public:
    DiffuseMaterial(TextureSampler* bump = nullptr) : Material(bump), color_(1.0f), sampler_(nullptr) {}
    DiffuseMaterial(const float4& color, TextureSampler* bump = nullptr) : Material(bump), color_(color), sampler_(nullptr) {}
    DiffuseMaterial(TextureSampler* sampler, TextureSampler* bump = nullptr) : Material(bump), sampler_(sampler) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        float4 color = color_;
        if (sampler_)
            color = sampler_->sample(isect.uv);

        auto bsdf = mem_arena.alloc<BSDF>(isect);
        auto brdf = mem_arena.alloc<Lambertian>(color);
        bsdf->add(brdf);

        return bsdf;
    }

private:
    float4 color_;
    TextureSampler* sampler_;
};

/// Simple mirror with perfect specular reflection.
class MirrorMaterial : public Material {
public:
    MirrorMaterial(float eta, float kappa, const float4& scale, TextureSampler* bump = nullptr)
        : Material(bump), fresnel_(eta, kappa), scale_(scale) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        auto bsdf = mem_arena.alloc<BSDF>(isect);
        auto brdf = mem_arena.alloc<SpecularReflection>(scale_, fresnel_);
        bsdf->add(brdf);
        return bsdf;
    }

    virtual bool is_specular() override { return true; }

private:
    FresnelConductor fresnel_;
    float4 scale_;
};

/// Simple glass material
class GlassMaterial : public Material {
public:
    GlassMaterial(float eta, const float4& transmittance, const float4& reflectance, TextureSampler* bump = nullptr)
        : Material(bump), eta_(eta), transmittance_(transmittance), reflectance_(reflectance), fresnel_(1.0f, eta) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        auto bsdf = mem_arena.alloc<BSDF>(isect);

        auto brdf = mem_arena.alloc<SpecularReflection>(reflectance_, fresnel_);
        bsdf->add(brdf);

        if (adjoint) {
            auto btdf = mem_arena.alloc<SpecularTransmission<true>>(eta_, 1.0f, transmittance_);
            bsdf->add(btdf);
        } else {
            auto btdf = mem_arena.alloc<SpecularTransmission<false>>(eta_, 1.0f, transmittance_);
            bsdf->add(btdf);
        }

        return bsdf;
    }

    virtual bool is_specular() override { return true; }

private:
    float eta_;
    float4 transmittance_;
    float4 reflectance_;
    FresnelDielectric fresnel_;
};

class GlossyMaterial : public Material {
public:
    GlossyMaterial(float exponent, const float4& specular_color, const float4& diffuse_color, TextureSampler* bump = nullptr)
        : Material(bump), exponent_(exponent), specular_color_(specular_color), diffuse_color_(diffuse_color), diff_sampler_(nullptr)
    {}

    GlossyMaterial(float exponent, const float4& specular_color, TextureSampler* diff_sampler, TextureSampler* bump = nullptr)
        : Material(bump), exponent_(exponent), specular_color_(specular_color), diffuse_color_(0.0f), diff_sampler_(diff_sampler)
    {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        float4 diff_color = diffuse_color_;
        if (diff_sampler_)
            diff_color = diff_sampler_->sample(isect.uv);

        auto bsdf = mem_arena.alloc<BSDF>(isect);

        auto fresnel = mem_arena.alloc<FresnelConductor>(1.0f, exponent_);
        auto spec_brdf = mem_arena.alloc<CookTorrance>(specular_color_, fresnel, exponent_);

        auto diff_brdf = mem_arena.alloc<Lambertian>(diff_color);

        bsdf->add(spec_brdf);
        bsdf->add(diff_brdf);

        return bsdf;
    }

private:
    float exponent_;
    float4 specular_color_;
    float4 diffuse_color_;
    TextureSampler* diff_sampler_;
};

}

#endif