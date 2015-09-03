#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height), ray_count_(width * height * 2)
{            
    // allocate memory for intersection data, written by traversal
    hits_ = thorin_new<Hit>(ray_count_);
    
    queues_[0].resize(ray_count_, s.state_len(), s.initial_state());
    queues_[1].resize(ray_count_, s.state_len(), s.initial_state());
}

imba::Image& imba::Render::operator() () {
    // generate the camera rays
    cur_queue_ = 0;
    ray_gen_(queues_[0]);

    while (queues_[cur_queue_].size()) {
        RayQueue::Entry ray_data = queues_[cur_queue_].pop();
        if (ray_data.ray_count < 64)
            break;
            
        traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_, ray_data.ray_count);
        shader_(ray_data.rays, hits_, ray_data.state_data, ray_data.pixel_indices, ray_data.ray_count, tex_, queues_[!cur_queue_]);
        
        cur_queue_ = !cur_queue_;
    }
    
    return tex_;
}
