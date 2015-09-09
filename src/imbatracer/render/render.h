#ifndef IMBA_RENDER_H
#define IMBA_RENDER_H

#include "image.h"
#include "camera.h"
#include "shader.h"

namespace imba {

class Render {
public:
    Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height);

    // renders the scene
    Image& operator() (int n_samples);
    
private:
    void clear_buffer();

    Shader& shader_;
    Camera& ray_gen_;
    
    ThorinVector<Node>& nodes_;
    ThorinVector<Vec4>& tris_;
    
    Image tex_;
    
    Hit* hits_[2];
    RayQueue queues_[3];
    int ray_count_;
    
    int state_len_;
    unsigned char* shader_mem_;
    
    RNG rng_;
};

} // namespace imba

#endif
