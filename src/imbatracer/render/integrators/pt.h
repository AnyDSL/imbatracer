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
public:
    PathTracer(Scene& scene) 
        : Integrator(scene), primary_rays(TARGET_RAY_COUNT, scene.traversal_data), shadow_rays(TARGET_RAY_COUNT, scene.traversal_data)
    {
    }
    
    virtual void render(Image& out) override;
    
private:
    RayQueue<PTState> primary_rays;
    RayQueue<RayState> shadow_rays;
};

} // namespace imba

#endif