#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "integrator.h"
#include "../ray_scheduler.h"
#include "../ray_gen.h"

//#define QUEUE_SCHEDULING

namespace imba {

struct PTState : RayState {
    rgb throughput;
    int bounces : 31;
    bool last_specular : 1;
};

/// Renders a scene using path tracing starting at the camera.
class PathTracer : public Integrator {
public:
    PathTracer(Scene& scene, PerspectiveCamera& cam, RayGen<PTState>& ray_gen, int max_path_len, int thread_count, int tile_size)
        : Integrator(scene, cam)
#ifdef QUEUE_SCHEDULING
        , scheduler_(ray_gen, scene)
#else
        , scheduler_(ray_gen, scene, thread_count, tile_size)
#endif
        , max_path_len_(max_path_len)
    {}

    virtual void render(AtomicImage& out) override;

private:
#ifdef QUEUE_SCHEDULING
    QueueScheduler<PTState, 8, 1> scheduler_;
#else
    TileScheduler<PTState, 1> scheduler_;
#endif

    const int max_path_len_;

    void process_shadow_rays(RayQueue<PTState>& ray_in, AtomicImage& out);
    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, AtomicImage& out);

    void compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf);
    void bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf);

    template<typename StateType, int queue_count, int shadow_queue_count, int max_shadow_rays_per_hit>
    friend class RayScheduler;
};

} // namespace imba

#endif // IMBA_PT_H
