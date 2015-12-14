#ifndef IMBA_BPT_H
#define IMBA_BPT_H

#include "integrator.h"

namespace imba {

struct BPTState : RayState {
    float4 throughput;
    int path_length;
};

struct LightRayState : RayState {
    float4 throughput;
    int path_length;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
};

// Ray generator for light sources. Samples a point and a direction on a lightsource for every pixel sample.
class BPTLightRayGen : public PixelRayGen<LightRayState> {
public:
    BPTLightRayGen(int w, int h, int n, LightContainer& lights) : PixelRayGen<LightRayState>(w, h, n), lights_(lights) { }

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, LightRayState& state_out) override {
        const float offset = 0.01f;

        // randomly choose one light source to sample
        int i = state_out.rng.random_int(0, lights_.size() - 1);
        auto& l = lights_[i];
        float pdf_lightpick = 1.0f / lights_.size();

        Light::LightRaySample sample = l->sample(state_out.rng);
        ray_out.org.x = sample.pos.x;
        ray_out.org.y = sample.pos.y;
        ray_out.org.z = sample.pos.z;
        ray_out.org.w = offset;

        ray_out.dir.x = sample.dir.x;
        ray_out.dir.y = sample.dir.y;
        ray_out.dir.z = sample.dir.z;
        ray_out.dir.w = FLT_MAX;

        float pdf_e = sample.pdf_emission_w * pdf_lightpick;

        state_out.throughput = sample.radiance / pdf_e;
        state_out.path_length = 1;

        state_out.dVC = sample.cos_out / pdf_e;
        state_out.dVCM = sample.pdf_direct_w * pdf_lightpick / pdf_e;
    }

private:
    LightContainer& lights_;
};

/// Generates camera rays for BPT.
class BPTCameraRayGen : public PixelRayGen<BPTState> {
public:
    BPTCameraRayGen(PerspectiveCamera& cam, int spp) : PixelRayGen<BPTState>(cam.width(), cam.height(), spp), cam_(cam) { }

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, BPTState& state_out) override {
        cam_.sample_pixel(x, y, ray_out, state_out.rng);
        state_out.throughput = float4(1.0f);
        state_out.path_length = 1;
    }

private:
    PerspectiveCamera& cam_;
};

// bidirectional path tracing
class BidirPathTracer : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 64 * 1000;
    constexpr static int MAX_LIGHT_PATH_LEN = 16;
public:
    BidirPathTracer(Scene& scene, PerspectiveCamera& cam, int spp)
        : Integrator(scene, cam, spp),
          width_(cam.width()),
          height_(cam.height()),
          n_samples_(spp),
          light_sampler_(width_, height_, n_samples_, scene.lights),
          camera_sampler_(cam, spp),
          primary_rays_ { RayQueue<BPTState>(TARGET_RAY_COUNT), RayQueue<BPTState>(TARGET_RAY_COUNT)},
          shadow_rays_(TARGET_RAY_COUNT * (MAX_LIGHT_PATH_LEN + 1)),
          light_rays_ { RayQueue<LightRayState>(TARGET_RAY_COUNT), RayQueue<LightRayState>(TARGET_RAY_COUNT)}
    {
        light_paths_.resize(width_ * height_);
        for (auto& p : light_paths_) {
            p.resize(n_samples_);
            for (auto& s : p) s.reserve(MAX_LIGHT_PATH_LEN);
        }

        camera_sampler_.set_target(TARGET_RAY_COUNT);
        light_sampler_.set_target(TARGET_RAY_COUNT);
    }

    virtual void render(Image& out) override;

private:
    int width_, height_;
    int n_samples_;

    BPTLightRayGen light_sampler_;
    BPTCameraRayGen camera_sampler_;

    struct LightPathVertex {
        Intersection isect;
        float4 throughput;

        // partial weights for MIS, see VCM technical report
        float dVC;
        float dVCM;

        LightPathVertex(Intersection isect, float4 tp, float dVC, float dVCM)
            : isect(isect), throughput(tp), dVC(dVC), dVCM(dVCM)
        {}
    };
    std::vector<std::vector<std::vector<LightPathVertex>>> light_paths_;

    void reset_buffers();

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

