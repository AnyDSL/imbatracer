#ifndef IMBA_CAMERA_H
#define IMBA_CAMERA_H

#include "ray_gen.h"
#include "ray_queue.h"
#include "random.h"

#include "../core/matrix.h"

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
        up = cross(dir, right);

        pos_ = pos;
        forward_ = dir;

        // Camera is represented by a matrix. The image plane is at such a distance from the position that the pixels have area one.
        const float3 local_p(dot(up, pos), dot(-right, pos), dot(-dir, pos));
        const float4x4 world_to_cam(float4(    up, -local_p.x),
                                    float4(-right, -local_p.y),
                                    float4(  -dir, -local_p.z),
                                    float4(0, 0, 0, 1));
        const auto& persp = perspective(fov_, width_ / height_, near_plane, far_plane);
        const auto& world_to_screen = persp * world_to_cam;
        const auto& screen_to_world = invert(world_to_screen);

        world_to_raster_ = scale(width_ * 0.5f, height_ * 0.5f, 0.0f, 1.0f) *
                           translate(1.0f, 1.0f, 0.0f) *
                           world_to_screen;

        raster_to_world_ = screen_to_world *
                           translate(-1.0f, -1.0f, 0.0f) *
                           scale(2.0f / width_, 2.0f / height_, 0.0f, 1.0f);

        const float tan_half = std::tan(fov_ * pi / 360.0f);
        img_plane_dist_ = width_ / (2.0f * tan_half);
    }

    Ray generate_ray(const float2& raster_pos) const {
        const auto w = raster_to_world(raster_pos);
        const float3 dir = normalize(w - pos_);

        return Ray {
            { pos_.x, pos_.y, pos_.z, 0.0f },
            { dir.x, dir.y, dir.z, FLT_MAX }
        };
    }

    Ray generate_ray(float x, float y) const {
        return generate_ray(float2(x,y));
    }

    float2 world_to_raster(const float3& world_pos) const {
        const auto t = project(world_to_raster_, world_pos);
        return float2(t.y, t.x);
    }

    float3 raster_to_world(const float2& raster_pos) const {
        return project(raster_to_world_, float3(raster_pos.y, raster_pos.x, 0));
    }

    int raster_to_id(const float2& pos) const {
        const int x = std::floor(pos.x);
        const int y = std::floor(pos.y);

        if (x < 0 || x >= width_ ||
            y < 0 || y >= height_)
            return -1;

        return y * width_ + x;
    }

    int width() const { return width_; }
    int height() const { return height_; }

    const float3& pos() const { return pos_; }
    const float3& dir() const { return forward_; }

    const float image_plane_dist() const { return img_plane_dist_; }

private:
    float width_;
    float height_;
    float fov_;

    float3 pos_;
    float3 forward_;
    float img_plane_dist_;

    float4x4 world_to_raster_;
    float4x4 raster_to_world_;
};

} // namespace imba

#endif // IMBA_CAMERA_H
