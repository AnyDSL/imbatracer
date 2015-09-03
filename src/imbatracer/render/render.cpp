#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height), rays_(width * height * 2, s.state_len(), s.initial_state()), ray_count_(width * height)
{            
    // allocate memory for intersection data, written by traversal
    hits_ = thorin_new<Hit>(ray_count_);
}

imba::Image& imba::Render::operator() () {
    //RayQueue rays_ = RayQueue(tex_.width() * tex_.height() * 2, shader_.state_len());
    // generate the camera rays
    ray_gen_(rays_);

    bool retrace = true;
    int i = 0;
    while (rays_.size()) {
        // traverse the acceleration structure
        RayQueue::Entry ray_data = rays_.pop();
        traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_, ray_data.ray_count);
        
        // shade the rays
        shader_(ray_data.rays, hits_, ray_data.state_data, ray_data.pixel_indices, ray_data.ray_count, tex_, rays_);
    }
    
    return tex_;
}
