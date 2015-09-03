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
    Image& operator() ();
    
private:
    Shader& shader_;
    Camera& ray_gen_;
    
    ThorinVector<Node>& nodes_;
    ThorinVector<Vec4>& tris_;
    
    Image tex_;
    
    Hit* hits_;
    RayQueue rays_;
    int ray_count_;
    
    int state_len_;
    unsigned char* shader_mem_;
};

} // namespace imba

#endif
