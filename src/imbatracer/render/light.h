#ifndef IMBA_LIGHT_H
#define IMBA_LIGHT_H

#include "../core/float3.h"

namespace imba {

class AreaLight {
public:
    AreaLight(const float3& point0, const float3& edge0, const float3& edge1, const float3& normal, const float4& intensity) 
        : point0_(point0), edge0_(edge0), edge1_(edge1), area_(length(cross(edge0, edge1))), intensity_(intensity), normal_(normal)
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
        // sample a point on the light source
        float3 pos = point0_ + u1 * edge0_ + u2 * edge1_;
        
        // compute distance and shadow ray direction
        s.dir = pos - from;
        float distsq = dot(s.dir, s.dir);
        s.distance = sqrtf(distsq);
        s.dir = s.dir * (1.0f / s.distance);
        
        float cos_normal_dir = dot(normal_, -1.0f * s.dir);
        
        // directions form the opposite side of the light have zero intensity
        if (cos_normal_dir > 0.0f && cos_normal_dir < 1.0f)
            s.intensity = intensity_ * cos_normal_dir * (area() / distsq);
        else
            s.intensity = float4(0.0f);
            
        return s;
    }
    
private:
    float3 point0_;
    float3 edge0_;
    float3 edge1_;
    float area_;
    float3 normal_;
    float4 intensity_;
};

} // namespace imba

#endif
