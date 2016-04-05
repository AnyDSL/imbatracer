#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "integrator.h"
#include "../ray_scheduler.h"
#include "../ray_gen.h"

namespace imba {

struct PTState : RayState {
    float4 throughput;
    int bounces;
    bool last_specular;
};

/// Renders a scene using path tracing starting at the camera.
class PathTracer : public Integrator {
public:
    PathTracer(Scene& scene, PerspectiveCamera& cam, RayGen<PTState>& ray_gen, int max_path_len = 32)
        : Integrator(scene, cam)
        , ray_gen_(ray_gen)
        , scheduler_(ray_gen, scene)
        , max_path_len_(max_path_len)
    {}

    virtual void render(AtomicImage& out) override;

private:
    //QueueScheduler<PTState, 8, 8, 1> scheduler_;
    TileScheduler<PTState, 4, 256, 1> scheduler_;
    RayGen<PTState>& ray_gen_;

    const int max_path_len_;

    void process_shadow_rays(RayQueue<PTState>& ray_in, AtomicImage& out);
    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, AtomicImage& out);

    void compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf);
    void bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf);

    template<typename StateType, int queue_count, int shadow_queue_count, int max_shadow_rays_per_hit>
    friend class RayScheduler;
};

} // namespace imba

#endif

