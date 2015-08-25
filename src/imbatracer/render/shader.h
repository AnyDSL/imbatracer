#ifndef IMBA_SHADER_H
#define IMBA_SHADER_H

#include "traversal.h"
#include "image.h"
#include "light.h"
#include <vector>

namespace imba {

class Shader {
public:
    // runs the shader on a set of rays / hit points
    // state == nullptr for primary rays
    virtual bool operator()(Ray* rays, Hit* hits, void* state, int ray_count, Image& out, Ray* ray_out, void* state_out) = 0;
    
    // returns the length (in bytes) of the state data stored per ray / intersection
    virtual int state_len() = 0;
};

class BasicPathTracer : public Shader {
    struct State {
        bool alive;
        float4 factor;
    };
public:
    BasicPathTracer(std::vector<AreaLight>& light_sources) : lights_(light_sources) { }
    
    virtual bool operator()(Ray* rays, Hit* hits, void* state, int ray_count, Image& out, Ray* ray_out, void* state_out) override;
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
private:
    std::vector<AreaLight>& lights_;
};

}

#endif
