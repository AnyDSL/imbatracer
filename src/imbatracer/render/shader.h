#ifndef IMBA_SHADER_H
#define IMBA_SHADER_H

#include "ray_queue.h"
#include "camera.h"
#include "image.h"
#include "light.h"
#include "random.h"
#include "scene.h"

namespace imba {

class Shader {
public:
    Shader(PixelRayGen& cam, LightContainer& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids)  
        : lights_(light_sources), tris_(tris), normals_(normals), materials_(materials), material_ids_(material_ids), cam_(cam) 
    {
    }

    virtual int num_passes() const = 0;
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) = 0;
    virtual PixelRayGen& get_ray_gen(int pass_id) = 0;
    
    // returns the length (in bytes) of the state data stored per ray / intersection
    virtual int state_len() = 0;
    virtual const char* initial_state() = 0;
    
protected:
    PixelRayGen& cam_;
    LightContainer& lights_;
    ThorinVector<Vec4>& tris_;
    std::vector<float3>& normals_;
    MaterialContainer& materials_;
    std::vector<int>& material_ids_;
};

// uses unidirectional path tracing starting from the camera
class PTShader : public Shader {    
    struct State {
        int pixel_idx;
        
        enum Kind {
            PRIMARY,
            SHADOW,
            SECONDARY
        };
        Kind kind;
        float4 factor;
    };
    
public:
    PTShader(PixelRayGen& cam, LightContainer& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids) 
        : Shader(cam, light_sources, tris, normals, materials, material_ids)
    {
        initial_state_.kind = State::PRIMARY;
        initial_state_.factor = float4(1.0f);
    }
    
    virtual int num_passes() const override { return 1; }
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) override;
    virtual PixelRayGen& get_ray_gen(int pass_id) override { return cam_; }
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
    virtual const char* initial_state() override {
        return reinterpret_cast<const char*>(&initial_state_);
    }
    
private:
    State initial_state_;
};
/*
// bidirectional path tracing
class BPTShader : public Shader {    
    struct State {
        int pixel_idx;
        
        enum Kind {
            PRIMARY,
            SECONDARY
        };
        Kind kind;
        float4 factor;
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
