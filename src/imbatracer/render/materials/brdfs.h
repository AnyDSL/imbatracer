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

}

#endif