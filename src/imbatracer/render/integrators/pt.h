#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "integrator.h"
#include "../ray_gen.h"

namespace imba {

struct PTState : RayState {
    float4 throughput;
    int bounces;
    bool last_specular;
};

/// Renders a scene using path tracing starting at the camera.
class PathTracer : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 1 << 20;
public:
    PathTracer(Scene& scene, PerspectiveCamera& cam, RayGen<PTState>& ray_gen)
        : Integrator(scene, cam),
          primary_rays_{ RayQueue<PTState>(TARGET_RAY_COUNT), RayQueue<PTState>(TARGET_RAY_COUNT) },
          shadow_rays_(TARGET_RAY_COUNT),
          ray_gen_(ray_gen)
    {}

    virtual void render(Image& out) override;

private:
    RayQueue<PTState> primary_rays_[2];
    RayQueue<PTState> shadow_rays_;
    RayGen<PTState>&  ray_gen_;

    void process_shadow_rays(RayQueue<PTState>& ray_in, Image& out);
    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, Image& out);

    void compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf);
    void bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf);
};

} // namespace imba

#endif

