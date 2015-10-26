#include "camera.h"
#include <cfloat>
#include <random>
#include "../core/float4.h"
#include "../core/common.h"

imba::PerspectiveCamera::PerspectiveCamera(int w, int h, int n_samples, float3 pos, float3 dir, float3 up, float fov)
    : PixelRayGen(w, h, n_samples), pos_(pos), aspect_(w / h)
{
    dir_ = normalize(dir);
    right_ = normalize(cross(dir_, up));
    up_ = normalize(cross(dir_, right_));
    
    float f = tanf(radians(fov / 2.0f));
    
    right_ = 2.0f * right_ * f * aspect_;
    up_ = 2.0f * up_ * f;
    
    top_left_ = dir_ - 0.5f * right_ - 0.5f * up_;
    step_x_ = right_ * (1.0f / static_cast<float>(w));
    step_y_ = up_ * (1.0f / static_cast<float>(h));
}

void imba::PerspectiveCamera::sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out) {
    float sample_x = static_cast<float>(x) + rng.random01();
    float sample_y = static_cast<float>(y) + rng.random01();
    
    float3 dir = top_left_ + sample_x * step_x_ + sample_y * step_y_;
    
    ray_out.org.x = pos_.x;
    ray_out.org.y = pos_.y;
    ray_out.org.z = pos_.z;
    ray_out.org.w = 0.0f;
    
    ray_out.dir.x = dir.x;
    ray_out.dir.y = dir.y;
    ray_out.dir.z = dir.z;
    ray_out.dir.w = FLT_MAX;
}
