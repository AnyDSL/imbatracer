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

}

#endif