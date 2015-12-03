#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "ray_gen.h"
#include "ray_queue.h"
#include "random.h"
#include "../core/float3.h"

namespace imba {

template <typename StateType>
class PerspectiveCamera : public PixelRayGen<StateType> {
public:
    PerspectiveCamera(int w, int h, int n_samples, float fov)
        : PixelRayGen<StateType>(w, h, n_samples), fov_(fov)
    {
        move(float3(0.0, 0.0f, -1.0f),
             float3(0.0f, 0.0f, 1.0f),
             float3(0.0f, 1.0f, 0.0f));
    }

    void move(float3 pos, float3 dir, float3 up) {
        float3 right = normalize(cross(dir, up));
        up = normalize(cross(dir, right));

        const float w = PixelRayGen<StateType>::width_;
        const float h = PixelRayGen<StateType>::height_;
        const float f = tanf(radians(fov_ / 2.0f));
        const float aspect = w / h;

        right = 2.0f * right * f * aspect;
        up = 2.0f * up * f;
        
        top_left_ = dir - 0.5f * right - 0.5f * up;
        step_x_ = right * (1.0f / w);
        step_y_ = up * (1.0f / h);
        pos_ = pos;
    }

private:
    float fov_;
    float3 pos_;
    float3 top_left_;
    float3 step_x_;
    float3 step_y_;
    
    void sample_pixel(int x, int y, ::Ray& ray_out, StateType& state_out) override {
        const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
        const float sample_y = static_cast<float>(y) + state_out.rng.random_float();

        const float3 dir = top_left_ + sample_x * step_x_ + sample_y * step_y_;

        ray_out.org.x = pos_.x;
        ray_out.org.y = pos_.y;
        ray_out.org.z = pos_.z;
        ray_out.org.w = 0.0f;
        
        ray_out.dir.x = dir.x;
        ray_out.dir.y = dir.y;
        ray_out.dir.z = dir.z;
        ray_out.dir.w = FLT_MAX;
    }
};

}

#endif
