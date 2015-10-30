#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "ray_queue.h"
#include "camera.h"
#include "image.h"
#include "light.h"
#include "random.h"
#include "scene.h"

namespace imba {

template <typename StateType>
class Integrator {
public:
    Integrator(PixelRayGen<StateType>& cam, LightContainer& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids)  
        : lights_(light_sources), tris_(tris), normals_(normals), materials_(materials), material_ids_(material_ids), cam_(cam) 
    {
    }

    virtual int num_passes() const = 0;
    virtual void shade(int pass_id, RayQueue<StateType>& rays, Image& out, RayQueue<StateType>& ray_out) = 0;
    virtual PixelRayGen<StateType>& get_ray_gen(int pass_id) = 0;
    
protected:
    PixelRayGen<StateType>& cam_;
    LightContainer& lights_;
    ThorinVector<Vec4>& tris_;
    std::vector<float3>& normals_;
    MaterialContainer& materials_;
    std::vector<int>& material_ids_;
};

struct PTState : RayState {
    float4 throughput;
    int bounces;
    bool last_specular;
    
    PTState() : throughput(1.0f), bounces(0), last_specular(false) { }
};

// uses unidirectional path tracing starting from the camera
class PathTracer : public Integrator<PTState> {    
public:
    PathTracer(PixelRayGen<PTState>& cam, LightContainer& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids) 
        : Integrator<PTState>(cam, light_sources, tris, normals, materials, material_ids)
    {
    }
    
    virtual int num_passes() const override { return 1; }
    virtual void shade(int pass_id, RayQueue<PTState>& rays, Image& out, RayQueue<PTState>& ray_out) override;
    virtual PixelRayGen<PTState>& get_ray_gen(int pass_id) override { return cam_; }
};
/*
// bidirectional path tracing
class BidirPathTracer : public Integrator {    
    struct State {
        int pixel_id;
        int sample_id;
        float4 throughput;
    };
    
    // Ray generator for light sources. Samples a point and a direction on a lightsource for every pixel sample.
    class LightRayGen : public PixelRayGen {
    public:
        LightRayGen(int w, int h, int n, LightContainer& lights) : PixelRayGen(w, h, n, LIGHT_RAY), lights_(lights) { }
        
        virtual void sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out) override { 
            // randomly choose one light source to sample
            int i = rng.random(0, lights_.size() - 1);
            auto& l = lights_[i];
            
            Light::LightRaySample sample = l->sample(rng);
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
        LightContainer& lights_;
    };

public:
    BPTShader(PixelRayGen& cam, LightContainer& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids) 
        : Shader(cam, light_sources, tris, normals, materials, material_ids), light_sampler_(cam.width(), cam.height(), cam.num_samples(), lights_)
    {
        initial_state_.kind = State::PRIMARY;
        initial_state_.factor = float4(1.0f);
    }
    
    virtual int num_passes() const override { return 2; }
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) override {
        if (pass_id == 0)
            shade_light_rays(rays, out, ray_out);
        else 
            shade_camera_rays(rays, out, ray_out);
    }
    
    virtual PixelRayGen& get_ray_gen(int pass_id) override { 
        if (pass_id == 0)
            return light_sampler_;
        else
            return cam_;
    }
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
    virtual const char* initial_state() override {
        return reinterpret_cast<const char*>(&initial_state_);
    }
    
private:
    State initial_state_;
    int target_ray_count;
    
    LightRayGen light_sampler_;
    
    void sample_lights(RayQueue& out);
    void shade_light_rays(RayQueue& rays, Image& out, RayQueue& ray_out);
    void shade_camera_rays(RayQueue& rays, Image& out, RayQueue& ray_out);
};*/

}

#endif
