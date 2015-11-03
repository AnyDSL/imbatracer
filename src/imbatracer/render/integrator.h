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
    virtual void start_pass(int pass_id) { }
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

struct BPTState : RayState {
    float4 throughput;
    int bounces;
    int light_id;
    
    BPTState() : bounces(0), throughput(1.0f) { }
};

// Ray generator for light sources. Samples a point and a direction on a lightsource for every pixel sample.
class BPTLightRayGen : public PixelRayGen<BPTState> {
public:
    BPTLightRayGen(int w, int h, int n, LightContainer& lights) : PixelRayGen<BPTState>(w, h, n, LIGHT_RAY), lights_(lights) { }
    
    virtual void sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out, BPTState& state_out) override { 
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
        
        state_out.light_id = i;
    }
    
private:
    LightContainer& lights_;
};

// bidirectional path tracing
class BidirPathTracer : public Integrator<BPTState> {        
public:
    BidirPathTracer(PixelRayGen<BPTState>& cam, LightContainer& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids,
                    int w, int h, int n_samples) 
        : Integrator<BPTState>(cam, light_sources, tris, normals, materials, material_ids), light_sampler_(cam.width(), cam.height(), cam.num_samples(), lights_), width_(w), height_(h), n_samples_(n_samples)
    {
        light_paths_.resize(w * h);
        for (auto& p : light_paths_) {
            p.resize(n_samples);
            for (auto& s : p) s.resize(max_light_path_length);
        }
        
        light_path_lengths_.resize(w * h);
        for (auto& p : light_path_lengths_) {
            p.resize(n_samples);
        }
    }
    
    virtual void start_pass(int pass_id) { 
        if (pass_id == 0) {
            for (auto& p : light_path_lengths_) {
                for (auto& s : p) s = 20;
            }    
        }
    }
    
    virtual int num_passes() const override { return 2; }
    virtual void shade(int pass_id, RayQueue<BPTState>& rays, Image& out, RayQueue<BPTState>& ray_out) override {
        if (pass_id == 0)
            shade_light_rays(rays, out, ray_out);
        else 
            shade_camera_rays(rays, out, ray_out);
    }
    
    virtual PixelRayGen<BPTState>& get_ray_gen(int pass_id) override { 
        if (pass_id == 0)
            return light_sampler_;
        else
            return cam_;
    }
    
private:
    constexpr static int max_light_path_length = 4;
    
    int width_, height_;
    int n_samples_;
    
    BPTLightRayGen light_sampler_;
    
    friend class BPTLightRayGen;
    struct LightPathVertex {
        float3 pos;
        int light_id;
        bool is_specular;
    };
    std::vector<std::vector<std::vector<LightPathVertex>>> light_paths_;
    std::vector<std::vector<int>> light_path_lengths_;
    
    void shade_light_rays(RayQueue<BPTState>& ray_in, Image& out, RayQueue<BPTState>& ray_out);
    void shade_camera_rays(RayQueue<BPTState>& ray_in, Image& out, RayQueue<BPTState>& ray_out);
};

}

#endif
