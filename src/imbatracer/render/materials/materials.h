#ifndef IMBA_MATERIALS_H
#define IMBA_MATERIALS_H

#include "imbatracer/render/materials/brdfs.h"
#include "imbatracer/render/materials/btdfs.h"

#include "imbatracer/render/light.h"
#include "imbatracer/render/texture_sampler.h"

namespace imba {

class Material {
public:
    Material(const TextureSampler* bump = nullptr,
             const AreaEmitter* emit = nullptr)
        : bump_(bump)
        , emit_(emit)
    {}

    // Duplicates the material
    virtual Material* duplicate() const = 0;

    virtual BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint = false) const = 0;

    /// Associates the material with a light source.
    void set_emitter(const AreaEmitter* e) { emit_.reset(e); }

    /// If the material is attached to a light source, this returns the lightsource, otherwise nullptr.
    const AreaEmitter* emitter() { return emit_.get(); }

    virtual bool is_specular() { return false; }

    /// Updates the shading normal of the given intersection using bump mapping.
    void bump(Intersection& isect) {
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

        isect.u_tangent = normalize(u);
        isect.v_tangent = normalize(v);
        isect.normal = cross(isect.u_tangent, isect.v_tangent);
    }

protected:
    const TextureSampler* bump_;
    std::unique_ptr<const AreaEmitter> emit_;
};

/// Very simple material with a lambertian BRDF.
class DiffuseMaterial : public Material {
public:
    DiffuseMaterial(const TextureSampler* bump = nullptr) : Material(bump), color_(1.0f), sampler_(nullptr) {}
    DiffuseMaterial(const rgb& color, const TextureSampler* bump = nullptr) : Material(bump), color_(color), sampler_(nullptr) {}
    DiffuseMaterial(const TextureSampler* sampler, const TextureSampler* bump = nullptr) : Material(bump), sampler_(sampler) {}

    virtual Material* duplicate() const override final {
        if (sampler_)
            return new DiffuseMaterial(sampler_, bump_);
        else
            return new DiffuseMaterial(color_, bump_);
    };

    BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        rgb color = color_;
        if (sampler_)
            color = sampler_->sample(isect.uv);

        auto brdf = mem_arena.alloc<Lambertian>(color);
        return mem_arena.alloc<BSDF>(isect, brdf, nullptr);
    }

private:
    rgb color_;
    const TextureSampler* sampler_;
};

/// Simple mirror with perfect specular reflection.
class MirrorMaterial : public Material {
public:
    MirrorMaterial(float eta, float kappa, const rgb& scale, TextureSampler* bump = nullptr)
        : Material(bump), fresnel_(eta, kappa), scale_(scale) {}

    MirrorMaterial(const MirrorMaterial& rhs)
        : Material(rhs.bump_), fresnel_(rhs.fresnel_), scale_(rhs.scale_)
    {}

    virtual Material* duplicate() const override final {
        return new MirrorMaterial(*this);
    };

    BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        auto brdf = mem_arena.alloc<SpecularReflection>(scale_, fresnel_);
        return mem_arena.alloc<BSDF>(isect, brdf, nullptr);
    }

    bool is_specular() override { return true; }

private:
    FresnelConductor fresnel_;
    rgb scale_;
};

/// Simple glass material
class GlassMaterial : public Material {
public:
    GlassMaterial(float eta, const rgb& transmittance, const rgb& reflectance, TextureSampler* bump = nullptr)
        : Material(bump), eta_(eta), transmittance_(transmittance), reflectance_(reflectance), fresnel_(1.0f, eta) {}

    GlassMaterial(const GlassMaterial& rhs)
        : Material(rhs.bump_), eta_(rhs.eta_), transmittance_(rhs.transmittance_), reflectance_(rhs.reflectance_), fresnel_(rhs.fresnel_)
    {}

    virtual Material* duplicate() const override final {
        return new GlassMaterial(*this);
    };

    BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
        auto brdf = mem_arena.alloc<SpecularReflection>(reflectance_, fresnel_);

        BxDF* btdf;
        if (adjoint)
            btdf = mem_arena.alloc<SpecularTransmission<true>>(eta_, 1.0f, transmittance_);
        else
            btdf = mem_arena.alloc<SpecularTransmission<false>>(eta_, 1.0f, transmittance_);

        return mem_arena.alloc<BSDF>(isect, brdf, btdf);
    }

    bool is_specular() override { return true; }

private:
    float eta_;
    rgb transmittance_;
    rgb reflectance_;
    FresnelDielectric fresnel_;
};

class GlossyMaterial : public Material {
public:
    GlossyMaterial(float exponent, const rgb& specular_color, const rgb& diffuse_color, const TextureSampler* bump = nullptr)
        : Material(bump), exponent_(exponent), specular_color_(specular_color), diffuse_color_(diffuse_color), diff_sampler_(nullptr)
    {}

    GlossyMaterial(float exponent, const rgb& specular_color, const TextureSampler* diff_sampler, const TextureSampler* bump = nullptr)
        : Material(bump), exponent_(exponent), specular_color_(specular_color), diffuse_color_(0.0f), diff_sampler_(diff_sampler)
    {}

    GlossyMaterial(const GlossyMaterial& rhs)
        : Material(rhs.bump_), exponent_(rhs.exponent_), specular_color_(rhs.specular_color_)
        , diffuse_color_(rhs.diffuse_color_), diff_sampler_(rhs.diff_sampler_)
    {}

    virtual Material* duplicate() const override final {
        return new GlossyMaterial(*this);
    };

    BSDF* get_bsdf(const Intersection& isect, MemoryArena& mem_arena, bool adjoint) const override {
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
    const TextureSampler* diff_sampler_;
};

} // namespace imba

#endif
