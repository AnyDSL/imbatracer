#ifndef IMBA_MATERIALS_H
#define IMBA_MATERIALS_H

#include "brdfs.h"
#include "btdfs.h"

#include "../light.h"
#include "../texture_sampler.h"

namespace imba {

class Material {
public:
    Material() : light_(nullptr) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena) const = 0;

    /// Associates the material with a light source.
    inline void set_light(Light* l) { light_ = l; }

    /// If the material is attached to a light source, this returns the lightsource, otherwise nullptr.
    inline Light* light() { return light_; }

private:
    Light* light_;
};

using MaterialContainer = std::vector<std::unique_ptr<Material>>;

/// Very simple material with a lambertian BRDF.
class DiffuseMaterial : public Material {
public:
    DiffuseMaterial() : color_(1.0f), sampler_(nullptr) {}
    DiffuseMaterial(const float4& color) : color_(color), sampler_(nullptr) {}
    DiffuseMaterial(TextureSampler* sampler) : sampler_(sampler) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena) const override {
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
    MirrorMaterial(float eta, float kappa, const float4& scale)
        : fresnel_(eta, kappa), scale_(scale) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena) const override {
        auto bsdf = mem_arena.alloc<BSDF>(isect);
        auto brdf = mem_arena.alloc<SpecularReflection>(scale_, fresnel_);
        bsdf->add(brdf);
        return bsdf;
    }

private:
    FresnelConductor fresnel_;
    float4 scale_;
};

/// Simple glass material
class GlassMaterial : public Material {
public:
    GlassMaterial(float eta, const float4& transmittance, const float4& reflectance)
        : eta_(eta), transmittance_(transmittance), reflectance_(reflectance), fresnel_(1.0f, eta) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena) const override {
        auto bsdf = mem_arena.alloc<BSDF>(isect);
        auto brdf = mem_arena.alloc<SpecularReflection>(reflectance_, fresnel_);
        auto btdf = mem_arena.alloc<SpecularTransmission>(eta_, 1.0f, transmittance_);

        bsdf->add(brdf);
        bsdf->add(btdf);

        return bsdf;
    }

private:
    float eta_;
    float4 transmittance_;
    float4 reflectance_;
    FresnelDielectric fresnel_;
};

class GlossyMaterial : public Material {
public:
    GlossyMaterial(float exponent, const float4& specular_color, const float4& diffuse_color)
        : exponent_(exponent), specular_color_(specular_color), diffuse_color_(diffuse_color), diff_sampler_(nullptr)
    {}

    GlossyMaterial(float exponent, const float4& specular_color, TextureSampler* diff_sampler)
        : exponent_(exponent), specular_color_(specular_color), diffuse_color_(0.0f), diff_sampler_(diff_sampler)
    {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena) const override {
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