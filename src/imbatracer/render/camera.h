#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "ray_gen.h"
#include "ray_queue.h"
#include "random.h"

#include "../core/float4x4.h"

namespace imba {

class PerspectiveCamera {
    static constexpr float near_plane = 0.1f;
    static constexpr float far_plane = 10000.0f;

public:
    PerspectiveCamera(int w, int h, float fov)
        : width_(w), height_(h), fov_(fov)
    {
        move(float3(0.0, 0.0f, -1.0f),
             float3(0.0f, 0.0f, 1.0f),
             float3(0.0f, 1.0f, 0.0f));
    }

    void move(float3 pos, float3 dir, float3 up) {
        dir = normalize(dir);
        float3 right = normalize(cross(dir, up));
        up = normalize(cross(dir, right));

        pos_ = pos;
        forward_ = dir;

        const float3 local_p(dot(up, pos), dot(-right, pos), dot(-dir, pos));
        const float4x4 world_to_cam(float4(    up, -local_p.x),
                                    float4(-right, -local_p.y),
                                    float4(  -dir, -local_p.z),
                                    float4(0.0f, 0.0f, 0.0f, 1.0f));
        const float4x4 persp = perspective_matrix(fov_, near_plane, far_plane);
        const float4x4 world_to_screen = persp * world_to_cam;
        const float4x4 screen_to_world = invert(world_to_screen);

        world_to_raster_ = scale_matrix(width_ * 0.5f, height_ * 0.5f, 0.0f) *
                           translate_matrix(1.0f, 1.0f, 0.0f) *
                           world_to_screen;

        raster_to_world_ = screen_to_world *
                           translate_matrix(-1.0f, -1.0f, 0.0f) *
                           scale_matrix(2.0f / width_, 2.0f / height_, 0.0f);
    }

    Ray generate_ray(const float2& raster_pos) {
        float3 w = raster_to_world(raster_pos);
        const float3 dir = normalize(w - pos_);

        return Ray {
            { pos_.x, pos_.y, pos_.z, 0.0f },
            { dir.x, dir.y, dir.z, FLT_MAX }
        };
    }

    Ray generate_ray(float x, float y) {
        return generate_ray(float2(x,y));
    }

    float2 world_to_raster(const float3& world_pos) {
        float3 t = transform_point(world_to_raster_, world_pos);
        return float2(t.y, t.x);
    }

    float3 raster_to_world(const float2& raster_pos) {
        float2 rp(raster_pos.y, raster_pos.x);
        return transform_point(raster_to_world_, float3(rp, 0.0f));
    }

    int get_pixel(const float2& pos) {
        int x = std::floor(pos.x);
        int y = std::floor(pos.y);

        if (x < 0 || x >= width_ ||
            y < 0 || y >= height_)
            return -1;

        return y * width_ + x;
    }

    int width() { return width_; }
    int height() { return height_; }

private:
    float width_;
    float height_;
    float fov_;

    float3 pos_;
    float3 forward_;

    float4x4 world_to_raster_;
    float4x4 raster_to_world_;
};

}

#endif
