#ifndef IMBA_BRDFS_H
#define IMBA_BRDFS_H

#include "bsdf.h"

namespace imba {

class Lambertian : public BxDF {
public:
    Lambertian(const float4& color) : BxDF(BxDFFlags(BSDF_DIFFUSE | BSDF_REFLECTION)), color_(color) {}

    virtual float4 eval(const float3& out_dir, const float3& in_dir) const override {
        return color_ * (1.0f / pi);
    }

private:
    float4 color_;
};

class SpecularReflection : public BxDF {
public:
    SpecularReflection(const float4& scale, const Fresnel& fresnel)
        : BxDF(BxDFFlags(BSDF_SPECULAR | BSDF_REFLECTION)), scale_(scale), fresnel_(fresnel) {}

    virtual float4 eval(const float3& out_dir, const float3& in_dir) const override {
        return float4(0.0f);
    }

    virtual float4 sample(const float3& out_dir, float3& in_dir, float rnd_num_1, float rnd_num_2, float& pdf) const {
        in_dir = float3(-out_dir.x, -out_dir.y, out_dir.z); // Reflected direction in shading space (normal == z.)
        pdf = 1.0f;

        return fresnel_.eval(cos_theta(out_dir)) * scale_ / fabsf(cos_theta(in_dir));
    }

    virtual float pdf(const float3& out_dir, const float3& in_dir) const {
        return 0.0f; // Probability between any two randomly choosen directions is zero due to delta distribution.
    }

private:
    float4 scale_;
    const Fresnel& fresnel_;
};

}

#endif