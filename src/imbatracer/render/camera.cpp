#include "camera.h"
#include <float.h>
#include "../core/float4.h"
#include "../core/common.h"

void imba::OrthographicCamera::operator()(RayQueue& out) {
    float world_width = 6.0f;
    float world_height = world_width / aspect_;
    float3 world_pos = float3(0.0f);
    float3 dir = float3(0.0f, 0.0f, 1.0f);

    for (int y = 0; y < height_; ++y) {
        float rely = -(static_cast<float>(y) / static_cast<float>(height_) - 0.5f) * 2.0f;
        for (int x = 0; x < width_; ++x) {
            float relx = (static_cast<float>(x) / static_cast<float>(width_) - 0.5f) * 2.0f;
            float3 offset = float3(relx * world_width * 0.5f, rely * world_height * 0.5f, 0.0f);
            float3 pos = world_pos + offset;
            
            Ray r;
            r.org.x = pos.x;
            r.org.y = pos.y;
            r.org.z = pos.z;
            r.org.w = 0.0f;
            
            r.dir.x = dir.x;
            r.dir.y = dir.y;
            r.dir.z = dir.z;
            r.dir.w = FLT_MAX;
            
            out.push(r, nullptr, y * width_ + x);
        }
    }
}

imba::PerspectiveCamera::PerspectiveCamera(int w, int h, float3 pos, float3 dir, float3 up, float fov)
    : Camera(w, h), pos_(pos)
{
    dir_ = normalize(dir);
    right_ = normalize(cross(dir_, up));
    up_ = cross(right_, dir_);
    
    float f = tanf(radians(fov / 2.0f));
    
    right_ = right_ * f * aspect_;
    up_ = up_ * f;
}

void imba::PerspectiveCamera::operator()(RayQueue& out) {
    for (int y = 0; y < height_; ++y) {
        float rely = 1.0f - (static_cast<float>(y) / static_cast<float>(height_ - 1)) * 2.0f;
        for (int x = 0; x < width_; ++x) {
            float relx = (static_cast<float>(x) / static_cast<float>(width_ - 1)) * 2.0f - 1.0f;
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
            
            out.push(r, nullptr, y * width_ + x);
        }
    }
}
