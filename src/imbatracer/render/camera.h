#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "ray_queue.h"
#include "../core/float3.h"
#include "random.h"

namespace imba {

class Camera {
public:
    Camera(int width, int height) : width_(width), height_(height) { aspect_ = width / height; }

    virtual void operator()(RayQueue& out, RNG& rng, int n_samples) = 0;
    
protected:    
    void sample_pixel(float& relx, float& rely, RNG& rng);
    
    int width_;
    int height_;
    float aspect_;
    
    float pixel_width_;
    float pixel_height_;
};

class OrthographicCamera : public Camera {
public:
    OrthographicCamera(int w, int h) : Camera(w, h) {}

    virtual void operator()(RayQueue& out, RNG& rng, int n_samples) override;
};

class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(int w, int h, float3 pos, float3 dir, float3 up, float fov);
    
    virtual void operator()(RayQueue& out, RNG& rng, int n_samples) override;
    
private:  
    float3 pos_;
    float3 dir_;
    float3 up_;
    float3 right_;
};

}

#endif
