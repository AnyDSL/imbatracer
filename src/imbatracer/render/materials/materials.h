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

        float du = 0.001f;
        float dv = 0.001f;
        float vscale = 0.02f;
        auto u_displace = bump_->sample(float2(isect.uv.x + du, isect.uv.y));
        auto v_displace = bump_->sample(float2(isect.uv.x, isect.uv.y + dv));
        auto displace   = bump_->sample(isect.uv);

        auto diff_u = vscale * (u_displace - displace)[0] / du;
        auto diff_v = vscale * (v_displace - displace)[0] / dv;

        auto n = cross(isect.v_tangent, isect.u_tangent);
        auto u = isect.u_tangent + diff_u * n;
        auto v = isect.v_tangent + diff_v * n;

        isect.normal = normalize(cross(u, v));
        isect.u_tangent = u;
        isect.v_tangent = v;
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
    DiffuseMaterial(const rgb& color, TextureSampler* bump = nullptr) : Material(bump), color_(color), sampler_(nullptr) {}
    DiffuseMaterial(TextureSampler* sampler, TextureSampler* bump = nullptr) : Material(bump), sampler_(sampler) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        rgb color = color_;
        if (sampler_)
            color = sampler_->sample(isect.uv);

        auto brdf = mem_arena.alloc<Lambertian>(color);
        return mem_arena.alloc<BSDF>(isect, brdf, nullptr);
    }

private:
    rgb color_;
    TextureSampler* sampler_;
};

/// Simple mirror with perfect specular reflection.
class MirrorMaterial : public Material {
public:
    MirrorMaterial(float eta, float kappa, const rgb& scale, TextureSampler* bump = nullptr)
        : Material(bump), fresnel_(eta, kappa), scale_(scale) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        auto brdf = mem_arena.alloc<SpecularReflection>(scale_, fresnel_);
        return mem_arena.alloc<BSDF>(isect, brdf, nullptr);
    }

    virtual bool is_specular() override { return true; }

private:
    FresnelConductor fresnel_;
    rgb scale_;
};

/// Simple glass material
class GlassMaterial : public Material {
public:
    GlassMaterial(float eta, const rgb& transmittance, const rgb& reflectance, TextureSampler* bump = nullptr)
        : Material(bump), eta_(eta), transmittance_(transmittance), reflectance_(reflectance), fresnel_(1.0f, eta) {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        auto brdf = mem_arena.alloc<SpecularReflection>(reflectance_, fresnel_);

        BxDF* btdf;
        if (adjoint)
            btdf = mem_arena.alloc<SpecularTransmission<true>>(eta_, 1.0f, transmittance_);
        else
            btdf = mem_arena.alloc<SpecularTransmission<false>>(eta_, 1.0f, transmittance_);

        return mem_arena.alloc<BSDF>(isect, brdf, btdf);
    }

    virtual bool is_specular() override { return true; }

private:
    float eta_;
    rgb transmittance_;
    rgb reflectance_;
    FresnelDielectric fresnel_;
};

class GlossyMaterial : public Material {
public:
    GlossyMaterial(float exponent, const rgb& specular_color, const rgb& diffuse_color, TextureSampler* bump = nullptr)
        : Material(bump), exponent_(exponent), specular_color_(specular_color), diffuse_color_(diffuse_color), diff_sampler_(nullptr)
    {}

    GlossyMaterial(float exponent, const rgb& specular_color, TextureSampler* diff_sampler, TextureSampler* bump = nullptr)
        : Material(bump), exponent_(exponent), specular_color_(specular_color), diffuse_color_(0.0f), diff_sampler_(diff_sampler)
    {}

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        rgb diff_color = diffuse_color_;
        if (diff_sampler_)
            diff_color = diff_sampler_->sample(isect.uv);

        auto fresnel = mem_arena.alloc<FresnelConductor>(1.0f, exponent_);
        auto spec_brdf = mem_arena.alloc<CookTorrance>(specular_color_, fresnel, exponent_);
        auto diff_brdf = mem_arena.alloc<Lambertian>(diff_color);

        auto brdf = mem_arena.alloc<CombineBxDF>(spec_brdf, diff_brdf);

        return mem_arena.alloc<BSDF>(isect, brdf, nullptr);
    }

private:
    float exponent_;
    rgb specular_color_;
    rgb diffuse_color_;
    TextureSampler* diff_sampler_;
};

}

#endif
