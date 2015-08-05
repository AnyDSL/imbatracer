#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height, int ray_count)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height)
{    
    if (!ray_count) 
        ray_count_ = width * height;
        
    rays_ = thorin_new<Ray>(ray_count_);
    hits_ = thorin_new<Hit>(ray_count_);
}

imba::Image& imba::Render::operator() () {
    // generate the camera rays
    ray_gen_(rays_, ray_count_);
    
    // traverse the acceleration structure
    traverse_accel(nodes_.data(), rays_, tris_.data(), hits_, ray_count_);
    
    // shade the rays
    shader_(rays_, hits_, ray_count_, tex_);
    
    return tex_;
}
