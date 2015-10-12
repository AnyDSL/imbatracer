#ifndef IMBA_SHADER_H
#define IMBA_SHADER_H

#include "ray_queue.h"
#include "camera.h"
#include "image.h"
#include "light.h"
#include "random.h"
#include "scene.h"

namespace imba {

class Shader {
public:
    Shader(Camera& cam, std::vector<AreaLight>& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids)  
        : lights_(light_sources), tris_(tris), normals_(normals), materials_(materials), material_ids_(material_ids), cam_(cam) 
    {
    }

    virtual void set_target_ray_count(int count) = 0;
    virtual void start_frame() = 0;
    virtual int num_passes() const = 0;
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) = 0;
    virtual void generate(int pass_id, RayQueue& out) = 0;
    
    // returns the length (in bytes) of the state data stored per ray / intersection
    virtual int state_len() = 0;
    virtual const char* initial_state() = 0;
    
protected:
    Camera& cam_;
    std::vector<AreaLight>& lights_;
    ThorinVector<Vec4>& tris_;
    std::vector<float3>& normals_;
    MaterialContainer& materials_;
    std::vector<int>& material_ids_;
};

class PTShader : public Shader {    
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
    PTShader(Camera& cam, std::vector<AreaLight>& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids) 
        : Shader(cam, light_sources, tris, normals, materials, material_ids)
    {
        initial_state_.kind = State::PRIMARY;
        initial_state_.factor = float4(1.0f);
    }
    
    virtual void set_target_ray_count(int count) override {
        cam_.set_target_count(count);
    }
    
    virtual int num_passes() const override { return 1; }
    virtual void start_frame() override { cam_.start_frame(); }
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) override;
    virtual void generate(int pass_id, RayQueue& out) override { cam_.fill_queue(out); }
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
    virtual const char* initial_state() override {
        return reinterpret_cast<const char*>(&initial_state_);
    }
    
private:
    State initial_state_;
};

class BPTShader : public Shader {    
    struct State {
        int pixel_idx;
        
        enum Kind {
            PRIMARY,
            SECONDARY
        };
        Kind kind;
        float4 factor;
    };
    
public:
    BPTShader(Camera& cam, std::vector<AreaLight>& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids) 
        : Shader(cam, light_sources, tris, normals, materials, material_ids)
    {
        initial_state_.kind = State::PRIMARY;
        initial_state_.factor = float4(1.0f);
    }
    
    virtual void set_target_ray_count(int count) override {
        cam_.set_target_count(count);
    }
    
    virtual int num_passes() const override { return 2; }
    virtual void start_frame() override { cam_.start_frame(); }
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) override;
    virtual void generate(int pass_id, RayQueue& out) override { cam_.fill_queue(out); }
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
    virtual const char* initial_state() override {
        return reinterpret_cast<const char*>(&initial_state_);
    }
    
private:
    State initial_state_;
};

class VCMShader : public Shader {    
    struct State {
        int pixel_idx;
        
        enum Kind {
            PRIMARY,
            SECONDARY
        };
        Kind kind;
        float4 factor;
    };
    
public:
    VCMShader(Camera& cam, std::vector<AreaLight>& light_sources, ThorinVector<Vec4>& tris, std::vector<float3>& normals, MaterialContainer& materials, std::vector<int>& material_ids) 
        : Shader(cam, light_sources, tris, normals, materials, material_ids)
    {
        initial_state_.kind = State::PRIMARY;
        initial_state_.factor = float4(1.0f);
    }
    
    virtual void set_target_ray_count(int count) override {
        cam_.set_target_count(count);
    }
    
    virtual int num_passes() const override { return 2; }
    virtual void start_frame() override { cam_.start_frame(); }
    virtual void shade(int pass_id, RayQueue& rays, Image& out, RayQueue& ray_out) override;
    virtual void generate(int pass_id, RayQueue& out) override { cam_.fill_queue(out); }
    
    virtual int state_len() override { 
        return sizeof(State);
    }
    
    virtual const char* initial_state() override {
        return reinterpret_cast<const char*>(&initial_state_);
    }
    
private:
    State initial_state_;
};


}

#endif
