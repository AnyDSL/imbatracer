#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height, int ray_count)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height)
{    
    if (!ray_count) 
        ray_count_ = width * height;
        
    // allocate memory for ray and intersection data, used by traversal
    rays_ = thorin_new<Ray>(ray_count_);
    hits_ = thorin_new<Hit>(ray_count_);
    
    // allocate buffers for the current state of a shader
    state_len_ = shader_.state_len() * ray_count_;
    shader_mem_ = new unsigned char[state_len_ * 2];
}

imba::Image& imba::Render::operator() () {
    // generate the camera rays
    ray_gen_(rays_, ray_count_);

    void* state_1_ = nullptr;
    void* state_2_ = shader_mem_;

    bool retrace = true;
    int i = 0;
    while (retrace) {
        // traverse the acceleration structure
        traverse_accel(nodes_.data(), rays_, tris_.data(), hits_, ray_count_);
        
        // shade the rays
        retrace = shader_(rays_, hits_, state_1_, ray_count_, tex_, rays_, state_2_);
            
        // swap the state buffers
        ++i;
        state_1_ = state_2_;
        state_2_ = (i % 2) * state_len_ + shader_mem_;
    }
    
    return tex_;
}
