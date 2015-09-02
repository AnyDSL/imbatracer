#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "../core/float3.h"

namespace imba {

class AreaLight {
public:
    AreaLight(const float3& s0, const float3& v0, const float3& v1, const float4& intensity) 
        : s0_(s0), v0_(v0), v1_(v1), area_(length(v0) * length(v1)), intensity_(intensity), normal_(normalize(cross(v0, v1)))
    {}

    float area() { return area_; }
    
    struct LightSample {
        float3 dir;
        float distance;
        float4 intensity;
    };
    
    // samples the light source, returns the pdf
    LightSample sample(const float3& from, float u1, float u2) { 
        LightSample s;
        float3 pos = s0_ + u1 * v0_ + u2 * v1_;
        s.dir = pos - from;
        float distsq = dot(s.dir, s.dir);
        s.distance = sqrtf(distsq);
        s.dir = s.dir * (1.0f / s.distance);
        s.intensity = intensity_ * dot(-1.0f * s.dir, normal_) * (area() / distsq);
        return s;
    }
    
private:
    float3 s0_;
    float3 v0_;
    float3 v1_;
    float area_;
    float3 normal_;
    float4 intensity_;
};

} // namespace imba

#endif
