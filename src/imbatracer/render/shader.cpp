#include "shader.h"
#include <float.h>
#include "../core/float4.h"
#include "../core/common.h"

#include <assert.h>

void imba::BasicPathTracer::operator()(Ray* rays, Hit* hits, void* state, int* pixel_indices, int ray_count, Image& out, RayQueue& ray_out) {
    State* shader_state = reinterpret_cast<State*>(state);
    
    auto rng = []() -> float { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };
    
    for (int i = 0; i < ray_count; ++i) {
        switch (shader_state[i].kind) {
        case State::PRIMARY:
            if (hits[i].tri_id != -1) {
                float3 pos = float3(rays[i].org.x, rays[i].org.y, rays[i].org.z);
                float3 rd = float3(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);

                float3 normal = normals_[hits[i].tri_id / 3];
                
                float epsilon = 0.001f;
                pos = pos + (hits[i].tmax) * rd + normal * epsilon;
                
                // calculate shadow ray direction (sample one point on one lightsource)
                auto ls = lights_[rng()];
                auto sample = ls.sample(pos, rng(), rng());
                float3 sh_dir = sample.dir;
                
                Ray ray;
                auto& org = ray.org;
                org.x = pos.x;
                org.y = pos.y;
                org.z = pos.z;
                org.w = 0.0f;
                
                auto& dir = ray.dir;
                dir.x = sh_dir.x;
                dir.y = sh_dir.y;
                dir.z = sh_dir.z;
                dir.w = sample.distance;
                
                float cos_term = fabsf(dot(sample.dir, normal));
                
                State s;
                s.kind = State::SHADOW;
                s.factor = cos_term * 1.0f / pi * sample.intensity;
                
                ray_out.push(ray, &s, pixel_indices[i]);
            }
            break;
            
        case State::SECONDARY:
            break;
            
        case State::SHADOW:
            float4 color = float4(0.0f);
            
            if (hits[i].tri_id == -1 || hits[i].tmax >= 1.0f)
                color = shader_state[i].factor;
        
            out.pixels()[pixel_indices[i] * 4] = color.x;
            out.pixels()[pixel_indices[i] * 4 + 1] = color.y;
            out.pixels()[pixel_indices[i] * 4 + 2] = color.z;
            break;
        }
    }
}
