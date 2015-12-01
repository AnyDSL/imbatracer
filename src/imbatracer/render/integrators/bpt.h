#ifndef IMBA_BPT_H
#define IMBA_BPT_H

#include "integrator.h"

namespace imba {

struct BPTState : RayState {
    float4 throughput;
    float4 contribution;
    int bounces;
    bool last_specular;
    
    BPTState() : bounces(0), throughput(1.0f), contribution(0.0f), last_specular(false) { }
};

struct LightRayState : RayState {
    float4 throughput;
    int bounces;
    int light_id;
    float4 power;
    
    LightRayState() : bounces(0), throughput(1.0f), light_id(-1), power(0.0f) { } 
};

// Ray generator for light sources. Samples a point and a direction on a lightsource for every pixel sample.
class BPTLightRayGen : public PixelRayGen<LightRayState> {
public:
    BPTLightRayGen(int w, int h, int n, LightContainer& lights) : PixelRayGen<LightRayState>(w, h, n), lights_(lights) { }
    
    virtual void sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out, LightRayState& state_out) override { 
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
        state_out.power = sample.intensity;
    }
    
private:
    LightContainer& lights_;
};

// bidirectional path tracing
class BidirPathTracer : public Integrator {  
    static constexpr int TARGET_RAY_COUNT = 64 * 1000;      
    constexpr static int MAX_LIGHT_PATH_LEN = 4;
public:
    BidirPathTracer(Scene& scene) 
        : Integrator(scene), 
          width_(static_cast<PixelRayGen<BPTState>*>(scene.camera)->width()), 
          height_(static_cast<PixelRayGen<BPTState>*>(scene.camera)->height()),
          n_samples_(static_cast<PixelRayGen<BPTState>*>(scene.camera)->num_samples()),
          light_sampler_(width_, height_, n_samples_, scene.lights),
          primary_rays_ { RayQueue<BPTState>(TARGET_RAY_COUNT), RayQueue<BPTState>(TARGET_RAY_COUNT)}, 
          shadow_rays_(TARGET_RAY_COUNT * MAX_LIGHT_PATH_LEN),
          light_rays_ { RayQueue<LightRayState>(TARGET_RAY_COUNT), RayQueue<LightRayState>(TARGET_RAY_COUNT)}
    {
        light_paths_.resize(width_ * height_);
        for (auto& p : light_paths_) {
            p.resize(n_samples_);
            for (auto& s : p) s.resize(MAX_LIGHT_PATH_LEN);
        }
        
        light_path_lengths_.resize(width_ * height_);
        for (auto& p : light_path_lengths_) {
            p.resize(n_samples_);
        }
        
        static_cast<PixelRayGen<BPTState>*>(scene.camera)->set_target_count(TARGET_RAY_COUNT);
        light_sampler_.set_target_count(TARGET_RAY_COUNT);
    }
    
    virtual void render(Image& out) override;
    
private:    
    int width_, height_;
    int n_samples_;
    
    BPTLightRayGen light_sampler_;
    
    struct LightPathVertex {
        float3 pos;
        int light_id;
        bool is_specular;
        float4 throughput;
        float4 power;
    };
    std::vector<std::vector<std::vector<LightPathVertex>>> light_paths_;
    std::vector<std::vector<int>> light_path_lengths_;
    
    void reset_light_paths();
    
    RayQueue<BPTState> primary_rays_[2];
    RayQueue<BPTState> shadow_rays_;
    RayQueue<LightRayState> light_rays_[2];
    
    void process_light_rays(RayQueue<LightRayState>& rays_in, RayQueue<LightRayState>& rays_out);
    void process_primary_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& shadow_rays, Image& img);
    void process_shadow_rays(RayQueue<BPTState>& rays_in, Image& img);
    
    void trace_light_paths();
    void trace_camera_paths(Image& img);
};

} // namespace imba

#endif