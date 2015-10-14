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
    
    // Samples a point on the light source. Used for shadow rays.
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
    
    struct LightRaySample {
        float3 pos;
        float3 dir;
        float4 intensity;  
    };
    
    LightRaySample sample(RNG& rng) {
        LightRaySample s;
        
        // sample a point and a direction on the light
        s.pos = point0_ + rng.random01() * edge0_ + rng.random01() * edge1_;
        DirectionSample ds = sample_hemisphere(normal_, rng.random01(), rng.random01());
        s.dir = ds.dir;
        s.intensity = intensity_ * (area() / ds.pdf);
        
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

class LightRayGen : public PixelRayGen {
public:
    LightRayGen(int w, int h, int n, std::vector<AreaLight>& lights) : PixelRayGen(w, h, n), lights_(lights) { }
    
    virtual void sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out) { 
        // randomly choose one light source to sample
        int i = rng.random(0, lights_.size());
        auto& l = lights_[i];
        
        AreaLight::LightRaySample sample = l.sample(rng);
        ray_out.org.x = sample.pos.x;
        ray_out.org.y = sample.pos.y;
        ray_out.org.z = sample.pos.z;
        ray_out.org.w = 0.0f;
        
        ray_out.dir.x = sample.dir.x;
        ray_out.dir.y = sample.dir.y;
        ray_out.dir.z = sample.dir.z;
        ray_out.dir.w = FLT_MAX;
    }
    
private:
    std::vector<AreaLight>& lights_;
};

} // namespace imba

#endif
