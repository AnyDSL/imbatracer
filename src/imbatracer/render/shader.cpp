#include "shader.h"
#include <float.h>
#include "../core/float4.h"
#include "../core/common.h"

bool imba::BasicPathTracer::operator()(Ray* rays, Hit* hits, void* state, int ray_count, Image& out, Ray* ray_out, void* state_out) {
    bool retrace = false;
    
    State* s_in = reinterpret_cast<State*>(state);
    State* s_out = reinterpret_cast<State*>(state_out);
    
    auto rng = []() -> float { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };
    
    if (!s_in) { // first iteration
        // Generate shadow rays
        for (int i = 0; i < ray_count; ++i) {
            if (hits[i].tri_id != -1) {
                float3 pos = float3(rays[i].org.x, rays[i].org.y, rays[i].org.z);
                float3 rd = float3(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
                
                float epsilon = -0.01f;
                pos = pos + (hits[i].tmax + epsilon) * rd;
                
                // calculate shadow ray direction (sample one point on one lightsource)
                auto ls = lights_[rng()];
                auto sample = ls.sample(rng(), rng());
                float3 sh_dir = sample.pos - pos;
                
                auto& org = ray_out[i].org;
                org.x = pos.x;
                org.y = pos.y;
                org.z = pos.z;
                org.w = 0.0f;
                
                auto& dir = ray_out[i].dir;
                dir.x = sh_dir.x;
                dir.y = sh_dir.y;
                dir.z = sh_dir.z;
                dir.w = FLT_MAX;
                
                float cos_term = fabsf(dot(sh_dir, rd));
                
                s_out[i].alive = true;
                s_out[i].factor = sample.pdf * cos_term * 1.0f / pi * sample.intensity;
            } else {
                s_out[i].alive = false;
                
                // create ray that will not intersect anything
                auto& org = ray_out[i].org;
                org.x = 0.0f;
                org.y = 0.0f;
                org.z = 0.0f;
                org.w = FLT_MAX; // tmin
                
                auto& dir = ray_out[i].dir;
                dir.x = 1.0f;
                dir.y = 0.0f;
                dir.z = 0.0f;
                dir.w = 0.0f; // tmax
            }
        }
        
        retrace = true;
    } else {
        for (int i = 0; i < ray_count; ++i) {
            if (s_in[i].alive) {
                float4 color = float4(0.0f);
                
                if (hits[i].tri_id == -1 || hits[i].tmax >= 1.0f)
                    color = s_in[i].factor;
            
                out.pixels()[i * 4] = color.x;
                out.pixels()[i * 4 + 1] = color.y;
                out.pixels()[i * 4 + 2] = color.y;
            }
            else {
                float4 color(0.5f, 0.0f, 0.0f, 0.0f);
                out.pixels()[i * 4] = color.x;
                out.pixels()[i * 4 + 1] = color.y;
                out.pixels()[i * 4 + 2] = color.y;
            }
        }
        
        retrace = false;
    }
    
    return retrace;
}
