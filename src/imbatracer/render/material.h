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
        emissive
    } kind;
    
    bool is_specular;
    
    Material(Kind k, bool specular) : kind(k), is_specular(specular) { }
    virtual ~Material() { }
};

struct SurfaceInfo {
    float3 normal;
    float u, v;  
};

class LambertMaterial : public Material {
public:
    LambertMaterial() : Material(lambert, false), diffuse_(1.0f, 0.0f, 1.0f, 1.0f), sampler_(nullptr) { }
    LambertMaterial(const float4& color) : Material(lambert, false), diffuse_(color), sampler_(nullptr) { }
    LambertMaterial(TextureSampler* sampler) : Material(lambert, false), sampler_(sampler) { }  
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
        float4 clr = diffuse_;
        if (sampler_) {
            clr = sampler_->sample(surf.u, surf.v);
        }
        
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(surf.normal, rnd_1, rnd_2);
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
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

// Perfect mirror reflection.
class MirrorMaterial : public Material {
public:
    MirrorMaterial() : Material(mirror, true) { }

    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
        // calculate the reflected direction
        in_dir = out_dir + 2.0f * surf.normal * dot(-1.0f * out_dir, surf.normal);
        pdf = 1.0f;
        return 1.0f;
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return 0.0f;
    }
};

// Material for diffuse emissive objects.
class EmissiveMaterial : public Material {
public:
    EmissiveMaterial(const float4& color) : color_(color), Material(emissive, false) { }
    
    inline float4 sample(const float3& out_dir, const SurfaceInfo& surf, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(surf.normal, rnd_1, rnd_2);
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        return color_ * (1.0f / pi);
    }
    
    inline float4 eval(const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
        return color_ * (1.0f / pi);
    }
    
    inline float4 color() { return color_; }
    
private:
    float4 color_;
};

inline float4 sample_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
    switch (mat->kind) {
    case Material::lambert:
        return static_cast<LambertMaterial*>(mat)->sample(out_dir, surf, rnd_1, rnd_2, in_dir, pdf);
    case Material::mirror:
        return static_cast<MirrorMaterial*>(mat)->sample(out_dir, surf, rnd_1, rnd_2, in_dir, pdf);
    case Material::emissive:
        return static_cast<EmissiveMaterial*>(mat)->sample(out_dir, surf, rnd_1, rnd_2, in_dir, pdf);
    }
}

inline float4 evaluate_material(Material* mat, const float3& out_dir, const SurfaceInfo& surf, const float3& in_dir) {
    switch (mat->kind) {
    case Material::lambert:
        return static_cast<LambertMaterial*>(mat)->eval(out_dir, surf, in_dir);
    case Material::mirror:
        return static_cast<MirrorMaterial*>(mat)->eval(out_dir, surf, in_dir);
    case Material::emissive:
        return static_cast<EmissiveMaterial*>(mat)->eval(out_dir, surf, in_dir);
    }
}

using MaterialContainer = std::vector<std::unique_ptr<imba::Material>>;

}

#endif
