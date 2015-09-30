#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "ray_queue.h"
#include "../core/float3.h"
#include "random.h"

namespace imba {

class Camera {
public:
    Camera(int width, int height, int n_samples) : width_(width), height_(height), n_samples_(n_samples) { aspect_ = width / height; }

    virtual void set_target_count(int count) = 0;
    
    virtual void start_frame() = 0;
    virtual void fill_queue(RayQueue& out) = 0;
    
protected:    
    void sample_pixel(float& relx, float& rely, RNG& rng);
    
    int width_;
    int height_;
    int n_samples_;
    float aspect_;
    
    float pixel_width_;
    float pixel_height_;
};

class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(int w, int h, int n_samples, float3 pos, float3 dir, float3 up, float fov);
    
    virtual void set_target_count(int count) override;
    
    virtual void start_frame() override;
    virtual void fill_queue(RayQueue& out) override;
    
private:  
    float3 pos_;
    float3 dir_;
    float3 up_;
    float3 right_;
    
    std::vector<::Ray> rays_;
    std::vector<int> pixels_;
    
    int next_pixel_;
    int generated_pixels_;
    int target_count_;
};

}

#endif
