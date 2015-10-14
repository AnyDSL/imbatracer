#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "ray_gen.h"
#include "ray_queue.h"
#include "../core/float3.h"
#include "random.h"

namespace imba {

class PerspectiveCamera : public PixelRayGen {
public:
    PerspectiveCamera(int w, int h, int n_samples, float3 pos, float3 dir, float3 up, float fov);
    
private:  
    float3 pos_;
    float3 dir_;
    float3 up_;
    float3 right_;
    
    float aspect_;
    
    float pixel_width_;
    float pixel_height_;
    
    virtual void sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out) override;
    void sample_pixel(float& relx, float& rely, RNG& rng);
};

}

#endif
