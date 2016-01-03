#ifndef IMBA_PT_H
#define IMBA_PT_H

#include "integrator.h"

namespace imba {

struct PTState : RayState {
    float4 throughput;
    int bounces;
    bool last_specular;
    
    PTState() : throughput(1.0f), bounces(0), last_specular(false) { }
};

/// Renders a scene using path tracing starting at the camera.
class PathTracer : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 64 * 1000;
public:
    PathTracer(Scene& scene, RayGen& cam)
        : Integrator(scene, cam),
          primary_rays { RayQueue<PTState>(TARGET_RAY_COUNT), RayQueue<PTState>(TARGET_RAY_COUNT) },
          shadow_rays(TARGET_RAY_COUNT)
    {
        static_cast<PixelRayGen<PTState>*>(&cam_)->set_target(TARGET_RAY_COUNT);
    }
    
    virtual void render(Image& out) override;
    
private:
    RayQueue<PTState> primary_rays[2];
    RayQueue<PTState> shadow_rays;
    
    void process_shadow_rays(RayQueue<PTState>& ray_in, Image& out);
    void process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, Image& out);
};

} // namespace imba

#endif
