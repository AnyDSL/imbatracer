#ifndef IMBA_SHADER_H
#define IMBA_SHADER_H

#include "ray_queue.h"
#include "image.h"
#include "light.h"
#include "random.h"
#include "materials/material.h"

namespace imba {

class Shader {
public:
    // runs the shader on a set of rays / hit points
    // state == nullptr for primary rays
    virtual void operator()(RayQueue& rays, Image& out, RayQueue& ray_out) = 0;
    
    // returns the length (in bytes) of the state data stored per ray / intersection
    virtual int state_len() = 0;
    
    virtual const char* initial_state() = 0;
};

class BasicPathTracer : public Shader {    
    struct State {
        int pixel_idx;
        
        enum Kind {
            PRIMARY,
            SHADOW,
            SECONDARY
        };
        Kind kind;
        float4 factor;
    };
    
public:
    BasicPathTracer(std::vector<AreaLight>& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, std::vector<Material>& materials, std::vector<int>& material_ids) 
        : lights_(light_sources), tris_(tris), normals_(normals), materials_(materials), material_ids_(material_ids)
    {
        initial_state_.kind = State::PRIMARY;
        initial_state_.factor = float4(1.0f);
    }
    
    virtual void operator()(RayQueue& rays, Image& out, RayQueue& ray_out) override;
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
    virtual const char* initial_state() {
        return reinterpret_cast<const char*>(&initial_state_);
    }
    
private:
    std::vector<AreaLight>& lights_;
    ThorinVector<Vec4>& tris_;
    std::vector<float3>& normals_;
    std::vector<Material>& materials_;
    std::vector<int>& material_ids_;
    
    State initial_state_;
};

}

#endif
