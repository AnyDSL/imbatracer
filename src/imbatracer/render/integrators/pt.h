#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "integrator.h"
#include "../ray_scheduler.h"

namespace imba {

struct PTState : RayState {
    float4 throughput;
    int bounces;
    bool last_specular;
};

/// Generates camera rays for PT.
class PTCameraRayGen : public PixelRayGen<PTState> {
public:
    PTCameraRayGen(PerspectiveCamera& cam, int spp) : PixelRayGen<PTState>(cam.width(), cam.height(), spp), cam_(cam) { }

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, PTState& state_out) override {
        float u1 = state_out.rng.random_float();
        float u2 = state_out.rng.random_float();
        const float sample_x = static_cast<float>(x) + u1;
        const float sample_y = static_cast<float>(y) + u2;

        ray_out = cam_.generate_ray(sample_x, sample_y);

        state_out.throughput = float4(1.0f);
        state_out.bounces = 0;
        state_out.last_specular = false;
    }

private:
    PerspectiveCamera& cam_;
};

/// Renders a scene using path tracing starting at the camera.
class PathTracer : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 2048 * 2048;//1 << 16;
public:
    PathTracer(Scene& scene, PerspectiveCamera& cam, int spp)
        : Integrator(scene, cam, spp)
        , ray_gen_(cam, spp)
        , scheduler_(ray_gen_, scene_)
    {
        //ray_gen_.set_target(TARGET_RAY_COUNT);
    }

    virtual void render(Image& out) override;

private:
    PTCameraRayGen ray_gen_;
    RayScheduler<PTState, 16> scheduler_;

    void process_shadow_rays(RayQueue<PTState>& ray_in, Image& out);
    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, Image& out);

    void compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf);
    void bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf);

    template<typename StateType, int queue_count>
    friend class RayScheduler;
};

} // namespace imba

#endif
