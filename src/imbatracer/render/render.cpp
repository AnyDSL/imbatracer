#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

#include <cassert>
#include <thread>

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height), ray_count_(width * height * 32)
{            
    // allocate memory for intersection data, written by traversal
    hits_[0] = thorin_new<Hit>(ray_count_);
    hits_[1] = thorin_new<Hit>(ray_count_);
    
    // we need 3 queues in order to do traversal and shading in parallel
    queues_[0].resize(ray_count_, s.state_len(), s.initial_state());
    queues_[1].resize(ray_count_, s.state_len(), s.initial_state());
    queues_[2].resize(ray_count_, s.state_len(), s.initial_state());
}

imba::Image& imba::Render::operator() (int n_samples) {
    assert(n_samples >= 2 && "number of samples must be at least 2");
    const int min_rays = std::min(tex_.width() * tex_.height(), 1000000);
    
    // generate and traverse the first set of rays
    int cur_q = 0; // next queue used as input for the shader
    int cur_hit = 0;
    ray_gen_(queues_[cur_q], rng_, 1);
    RayQueue::Entry ray_data = queues_[cur_q].peek();
    traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_[cur_hit], ray_data.ray_count);
    
    int created_samples = 1;
    while (created_samples <= n_samples) {
        // prepare a second queue for traversal
        // if there are not enough rays in the queue yet, fill up with new samples from the camera
        int traversal_q = (cur_q + 1) % 3; 
        if (queues_[traversal_q].size() < min_rays) {
            ray_gen_(queues_[traversal_q], rng_, 1);
            created_samples++;
        }
        
        // launch a thread to traverse the second queue
        std::thread traversal([=] () {
            RayQueue::Entry ray_data = queues_[traversal_q].peek();
            traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_[!cur_hit], ray_data.ray_count);
        });
        
        // shade the first queue
        int shader_out_q = (cur_q + 2) % 3;
        RayQueue::Entry ray_data = queues_[cur_q].pop();
        shader_(ray_data.rays, hits_[cur_hit], ray_data.state_data, ray_data.pixel_indices, ray_data.ray_count, tex_, queues_[shader_out_q], rng_);
        
        // wait for traversal to finish
        traversal.join();
        
        // rotate the queues and swap the hit buffers
        cur_q = traversal_q;
        cur_hit = !cur_hit;
    }
    
    // remove leftover rays from all queues (so the next frame starts fresh)
    for (int i = 0; i < 3; ++i)
        queues_[i].pop();
    
    return tex_;
}

void imba::Render::clear_buffer() {
    const int size = tex_.width() * tex_.height() * 4;
    for (int i = 0; i < size; i++) {
        tex_.pixels()[i] = 0.0f;
    }
}
