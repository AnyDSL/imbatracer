#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

#include <iostream>

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height), ray_count_(width * height * 10)
{            
    // allocate memory for intersection data, written by traversal
    hits_ = thorin_new<Hit>(ray_count_);
    
    queues_[0].resize(ray_count_, s.state_len(), s.initial_state());
    queues_[1].resize(ray_count_, s.state_len(), s.initial_state());
    
    cur_queue_ = 0;
}

imba::Image& imba::Render::operator() () {
    const int min_rays = std::min(tex_.width() * tex_.height(), 100000);

    clear_buffer();
    
    // generate the camera rays
    ray_gen_(queues_[cur_queue_]);

    while (queues_[cur_queue_].size() > min_rays) {
        RayQueue::Entry ray_data = queues_[cur_queue_].pop();
        
        //std::cout << "processing " << ray_data.ray_count << " rays..." << std::endl;
            
        traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_, ray_data.ray_count);
        shader_(ray_data.rays, hits_, ray_data.state_data, ray_data.pixel_indices, ray_data.ray_count, tex_, queues_[!cur_queue_]);
        
        cur_queue_ = !cur_queue_;
    }
    
    //std::cout << "finished frame" << std::endl;
    
    return tex_;
}

void imba::Render::clear_buffer() {
    const int size = tex_.width() * tex_.height() * 4;
    for (int i = 0; i < size; i++) {
        tex_.pixels()[i] = 0.0f;
    }
}
