#include "camera.h"
#include <cfloat>
#include <random>
#include "../core/float4.h"
#include "../core/common.h"

#include <iostream>

imba::PerspectiveCamera::PerspectiveCamera(int w, int h, int n_samples, float3 pos, float3 dir, float3 up, float fov)
    : Camera(w, h, n_samples), pos_(pos)
{
    dir_ = normalize(dir);
    right_ = normalize(cross(dir_, up));
    up_ = cross(right_, dir_);
    
    float f = tanf(radians(fov / 2.0f));
    
    right_ = right_ * f * aspect_;
    up_ = up_ * f;
    
    pixel_width_ = length(right_) / static_cast<float>(w);
    pixel_height_ = length(up_) / static_cast<float>(h);
}

void imba::PerspectiveCamera::set_target_count(int count) {
    pixels_.resize(count);
    rays_.resize(count);
    target_count_ = count;
}

void imba::PerspectiveCamera::start_frame() {
    next_pixel_ = 0;
    generated_pixels_ = 0;
}

void imba::PerspectiveCamera::fill_queue(RayQueue& out) {
    thread_local RNG rng;
    
    // only generate at most n samples per pixel
    if (generated_pixels_ > n_samples_ * width_ * height_) return;
    
    int count = target_count_ - out.size();
    if (count <= 0) return;
    
    if (generated_pixels_ + count > n_samples_ * width_ * height_) {
        count = n_samples_ * width_ * height_ - generated_pixels_;
    }
    
    generated_pixels_ += count;
    
    const int last_pixel = (next_pixel_ + count) % (width_ * height_);
    
#pragma omp parallel for
    for (int i = next_pixel_; i < next_pixel_ + count; ++i) {
        int pixel_idx = i % (width_ * height_);
        
        int y = pixel_idx / width_;
        int x = pixel_idx % width_;
        
        float rely = 1.0f - (static_cast<float>(y) / static_cast<float>(height_ - 1)) * 2.0f;
        float relx = (static_cast<float>(x) / static_cast<float>(width_ - 1)) * 2.0f - 1.0f;
        sample_pixel(relx, rely, rng);
        
        float3 dir = dir_ + right_ * relx + up_ * rely;
                
        Ray r;
        r.org.x = pos_.x;
        r.org.y = pos_.y;
        r.org.z = pos_.z;
        r.org.w = 0.0f;
        
        r.dir.x = dir.x;
        r.dir.y = dir.y;
        r.dir.z = dir.z;
        r.dir.w = FLT_MAX;

        pixels_[i - next_pixel_] = pixel_idx;
        rays_[i - next_pixel_] = r;
    }
    
    // store which pixel has to be sampled next
    next_pixel_ = last_pixel;
    
    out.push(rays_.begin(), rays_.begin() + count, pixels_.begin(), pixels_.begin() + count);
}

void imba::Camera::sample_pixel(float& relx, float& rely, RNG& rng) {
    relx += pixel_width_ * rng.random(-0.5, 0.49);
    rely += pixel_height_ * rng.random(-0.5, 0.49);
}
