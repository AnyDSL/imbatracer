#ifndef IMBA_MATERIAL_H
#define IMBA_MATERIAL_H

#include "../core/float3.h"
#include "../core/common.h"
#include "random.h"
#include <iostream>
namespace imba {

struct Material {
    enum Kind {
        lambert,
        mirror
    } kind;
    
    Material(Kind k) : kind(k) { }
    virtual ~Material() { }
};

class LambertMaterial : public Material {
public:
    LambertMaterial() : Material(lambert), diffuse(1.0f, 0.0f, 1.0f, 1.0f) { }
    LambertMaterial(const float4& color) : Material(lambert), diffuse(color) { }    
    
    inline float4 sample(const float3& out_dir, const float3& normal, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
        // uniform sample the hemisphere
        DirectionSample hemi_sample = sample_hemisphere(normal, rnd_1, rnd_2);
        in_dir = hemi_sample.dir;
        pdf = hemi_sample.pdf;
        return diffuse * (1.0f / pi);
    }
    
    inline float4 eval(const float3& out_dir, const float3& normal, const float3& in_dir) {
        return diffuse * (1.0f / pi); 
    }
    
private:
    float4 diffuse;
};

class MirrorMaterial : public Material {
public:
    MirrorMaterial() : Material(mirror) { }

    inline float4 sample(const float3& out_dir, const float3& normal, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
        // calculate the reflected direction
        in_dir = out_dir + 2.0f * normal * dot(-1.0f * out_dir, normal);
        pdf = 1.0f;
        return 1.0f;
    }
    
    inline float4 eval(const float3& out_dir, const float3& normal, const float3& in_dir) {
        return 0.0f;
    }
    
private:
    static constexpr float epsilon = 0.1f;
};

inline float4 sample_material(Material* mat, const float3& out_dir, const float3& normal, float rnd_1, float rnd_2, float3& in_dir, float& pdf) {
    switch (mat->kind) {
    case Material::lambert:
        return static_cast<LambertMaterial*>(mat)->sample(out_dir, normal, rnd_1, rnd_2, in_dir, pdf);
    case Material::mirror:
        return static_cast<MirrorMaterial*>(mat)->sample(out_dir, normal, rnd_1, rnd_2, in_dir, pdf);
    }
}

inline float4 evaluate_material(Material* mat, const float3& out_dir, const float3& normal, const float3& in_dir) {
    switch (mat->kind) {
    case Material::lambert:
        return static_cast<LambertMaterial*>(mat)->eval(out_dir, normal, in_dir);
    case Material::mirror:
        return static_cast<MirrorMaterial*>(mat)->eval(out_dir, normal, in_dir);
    }
}

}

#endif
