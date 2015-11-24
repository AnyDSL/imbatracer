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
        transparent,
        combine
    } kind;
    
    Material(Kind k) : kind(k) { }
    virtual ~Material() { }
};

struct SurfaceInfo {
    float3 normal;
    float u, v;  
};

float4 evaluate_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir);
float4 sample_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular);

class LambertMaterial : public Material {
public:
    LambertMaterial() : Material(lambert), diffuse_(1.0f, 0.0f, 1.0f, 1.0f), sampler_(nullptr) { }
    LambertMaterial(const float4& color) : Material(lambert), diffuse_(color), sampler_(nullptr) { }
    LambertMaterial(TextureSampler* sampler) : Material(lambert), sampler_(sampler) { }  
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(surf.u, surf.v);
        }
        
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(surf.normal, rng.random01(), rng.random01());
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        specular = false;
        return clr * (1.0f / pi);
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(surf.u, surf.v);
        }
        
        return clr * (1.0f / pi); 
    }
    
private:
    float4 diffuse_;
    TextureSampler* sampler_;
};

class TransparentMaterial : public Material {
public:
    TransparentMaterial() : Material(transparent) { }
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        in_dir = out_dir;
        specular = true;
        pdf = 1.0f;
        return 1.0f / fabsf(dot(out_dir, surf.normal));
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return 0.0f; 
    }
};

/// Combines two materials together using weights from a texture. 0 => full contribution from the first material
/// 1 => full contribution from the second material
class CombineMaterial : public Material {
public:
    CombineMaterial(TextureSampler* scale, std::unique_ptr<Material> m1, std::unique_ptr<Material> m2) 
        : Material(combine), scale_(scale), m1_(std::move(m1)), m2_(std::move(m2)) { }
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        const float s = scale_->sample(surf.u, surf.v).x;
        const float r = rng.random01();
        if (r < s) {
            return sample_material(m1_.get(), out_dir, surf, rng, in_dir, pdf, specular);
        } else {
            return sample_material(m2_.get(), out_dir, surf, rng, in_dir, pdf, specular);
        }
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        const float s = scale_->sample(surf.u, surf.v).x;
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
    MirrorMaterial() : Material(mirror) { }

    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        // calculate the reflected direction
        in_dir = out_dir + 2.0f * surf.normal * dot(-1.0f * out_dir, surf.normal);
        pdf = 1.0f;
        specular = true;
        return 1.0f;
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return 0.0f;
    }
};

/// Material for diffuse emissive objects.
class EmissiveMaterial : public Material {
public:
    EmissiveMaterial(const float4& color) : color_(color), Material(emissive) { }
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, RNG& rng, float3& in_dir, float& pdf, bool& specular) {
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(surf.normal, rng.random01(), rng.random01());
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
    HANDLE_MATERIAL(Material::transparent, TransparentMaterial) \
    HANDLE_MATERIAL(Material::combine, CombineMaterial)

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
