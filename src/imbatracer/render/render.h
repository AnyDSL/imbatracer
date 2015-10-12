#ifndef IMBA_RENDER_H
#define IMBA_RENDER_H

#include "image.h"
#include "camera.h"
#include "shader.h"

namespace imba {

class Render {
public:
    Render(ThorinVector<::Node>& nodes, ThorinVector<::Vec4>& tris, Shader& s, int width, int height);

    // renders the scene
    Image& operator() (int n_samples);
    
private:
    void clear_texture();

    Shader& shader_;    
    Image tex_;
    
    Hit* hits_[2];
    RayQueue queues_[3];
    int target_ray_count_;
    
    int state_len_;
    unsigned char* shader_mem_;
};

} // namespace imba

#endif
