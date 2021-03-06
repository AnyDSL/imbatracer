#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/scheduling/ray_scheduler.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

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
    PathTracer(Scene& scene, PerspectiveCamera& cam, RayScheduler<PTState, ShadowState>& scheduler, int max_path_len)
        : Integrator(scene, cam)
        , scheduler_(scheduler)
        , max_path_len_(max_path_len)
    {}

    virtual void render(AtomicImage& out) override;

private:
    RayScheduler<PTState, ShadowState>& scheduler_;

    const int max_path_len_;

    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<ShadowState>& ray_out_shadow, AtomicImage& out);

    void compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<ShadowState>& ray_out_shadow, BSDF* bsdf);
    void bounce(const Intersection& isect, PTState& state_out, Ray& ray_out, BSDF* bsdf, float offset);
};

} // namespace imba

#endif // IMBA_PT_H
