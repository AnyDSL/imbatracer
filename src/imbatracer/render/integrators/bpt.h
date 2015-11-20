#ifndef IMBA_BPT_H
#define IMBA_BPT_H
/*
#include "integrator.h"

namespace imba {

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
    BidirPathTracer(Scene<BPTState>& scene, int w, int h, int n_samples) 
        : Integrator<BPTState>(scene), 
          width_(scene.camera.width()), height_(scene.camera.height()), n_samples_(scene.camera.num_samples()),
          light_sampler_(width_, height_, n_samples_, scene.lights)
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
            return scene_.camera;
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

} // namespace imba
*/
#endif