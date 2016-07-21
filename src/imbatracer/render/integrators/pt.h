#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "integrator.h"
#include "../ray_scheduler.h"
#include "../ray_gen.h"

namespace imba {

struct PTState : RayState {
    rgb throughput;
    int bounces : 31;
    bool last_specular : 1;
    float last_pdf;
};

/// Renders a scene using path tracing starting at the camera.
class PathTracer : public Integrator {
public:
    PathTracer(Scene& scene, PerspectiveCamera& cam, RayScheduler<PTState>& scheduler, int max_path_len)
        : Integrator(scene, cam)
        , scheduler_(scheduler)
        , max_path_len_(max_path_len)
    {}

    virtual void render(AtomicImage& out) override;

private:
    RayScheduler<PTState>& scheduler_;

    const int max_path_len_;

    void process_shadow_rays(RayQueue<PTState>& ray_in, AtomicImage& out);
    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, AtomicImage& out);

    void compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf);
    void bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf);
};

} // namespace imba

#endif // IMBA_PT_H
