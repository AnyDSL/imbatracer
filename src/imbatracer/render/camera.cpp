#include "camera.h"
#include <cfloat>
#include <random>
#include "../core/float4.h"
#include "../core/common.h"

#include <iostream>

imba::PerspectiveCamera::PerspectiveCamera(int w, int h, int n_samples, float3 pos, float3 dir, float3 up, float fov)
    : PixelRayGen(w, h, n_samples), pos_(pos), aspect_(w / h)
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

void imba::PerspectiveCamera::sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out) {
    float rely = 1.0f - (static_cast<float>(y) / static_cast<float>(height_ - 1)) * 2.0f;
    float relx = (static_cast<float>(x) / static_cast<float>(width_ - 1)) * 2.0f - 1.0f;
    sample_pixel(relx, rely, rng);
    
    float3 dir = dir_ + right_ * relx + up_ * rely;
    
    ray_out.org.x = pos_.x;
    ray_out.org.y = pos_.y;
    ray_out.org.z = pos_.z;
    ray_out.org.w = 0.0f;
    
    ray_out.dir.x = dir.x;
    ray_out.dir.y = dir.y;
    ray_out.dir.z = dir.z;
    ray_out.dir.w = FLT_MAX;
}

void imba::PerspectiveCamera::sample_pixel(float& relx, float& rely, RNG& rng) {
    relx += pixel_width_ * rng.random(-0.5f, 0.49f);
    rely += pixel_height_ * rng.random(-0.5f, 0.49f);
}
