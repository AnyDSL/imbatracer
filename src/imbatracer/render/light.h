#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "../core/float3.h"

namespace imba {

class AreaLight {
public:
    AreaLight(float3 s0, float3 v0, float3 v1, float4 intensity) 
        : s0_(s0), v0_(v0), v1_(v1), area_(length(v0) * length(v1)), intensity_(intensity)
    {}

    float area() { return area_; }
    
    struct LightSample {
        float pdf;
        float3 pos;
        float4 intensity;
    };
    
    // samples the light source, returns the pdf
    LightSample sample(float u1, float u2) { 
        LightSample s;
        s.pos = s0_ + u1 * v0_ + u2 * v1_;
        s.pdf = 1.0f / area();
        s.intensity = intensity_;
        return s;
    }
    
private:
    float3 s0_;
    float3 v0_;
    float3 v1_;
    float area_;
    float4 intensity_;
};

} // namespace imba

#endif
