#include "shader.h"
#include "../core/float4.h"
#include "../core/common.h"
#include "random.h"

#include <float.h>
#include <assert.h>

#include <iostream>

void imba::BasicPathTracer::operator()(Ray* rays, Hit* hits, void* state, int* pixel_indices, int ray_count, Image& out, RayQueue& ray_out) {
    State* shader_state = reinterpret_cast<State*>(state);
    
    auto rng = []() -> float { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };
    
    for (int i = 0; i < ray_count; ++i) {
        switch (shader_state[i].kind) {
        case State::PRIMARY:
        case State::SECONDARY:
            if (hits[i].tri_id != -1) {
                float3 pos = float3(rays[i].org.x, rays[i].org.y, rays[i].org.z);
                float3 rd = float3(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);

                float3 normal = normals_[hits[i].tri_id / 3];
                pos = pos + (hits[i].tmax) * rd;
                
                // calculate shadow ray direction (sample one point on one lightsource)
                auto ls = lights_[rng()];
                auto sample = ls.sample(pos, rng(), rng());
                float3 sh_dir = sample.dir;
                
                Ray ray;
                auto& org = ray.org;
                org.x = pos.x;
                org.y = pos.y;
                org.z = pos.z;
                org.w = 0.001;
                
                auto& dir = ray.dir;
                dir.x = sh_dir.x;
                dir.y = sh_dir.y;
                dir.z = sh_dir.z;
                dir.w = sample.distance - 0.001;
                
                float cos_term = fabsf(dot(sample.dir, normal));
                
                State s;
                s.kind = State::SHADOW;
                s.factor = shader_state[i].factor * (cos_term * 1.0f / pi * sample.intensity);
                
                ray_out.push(ray, &s, pixel_indices[i]);
                
                // continue path using russian roulette
                float rrprob = 0.7f;
                float u_rr = rng();
                if (u_rr < rrprob) {
                    // sample hemisphere
                    float3 dir = normalize(sample_hemisphere(normal, rng(), rng()));
                    float sample_pdf = 1.f / pi;
                    
                    float cos_term = fabsf(dot(normal, dir));
                    
                    State s;
                    s.kind = State::SECONDARY;
                    s.factor = shader_state[i].factor * (cos_term * (1.0f / pi) / (rrprob * sample_pdf));
                    
                    Ray ray;
                    ray.org.x = pos.x;
                    ray.org.y = pos.y;
                    ray.org.z = pos.z;
                    ray.org.w = 0.001;
                    
                    ray.dir.x = dir.x;
                    ray.dir.y = dir.y;
                    ray.dir.z = dir.z;
                    ray.dir.w = FLT_MAX;
                    ray_out.push(ray, &s, pixel_indices[i]);
                }
            }
            break;
            
        case State::SHADOW:
            float4 color = float4(0.0f);
            
            if (hits[i].tri_id == -1)
                color = shader_state[i].factor;
        
            out.pixels()[pixel_indices[i] * 4] = color.x;
            out.pixels()[pixel_indices[i] * 4 + 1] = color.y;
            out.pixels()[pixel_indices[i] * 4 + 2] = color.z;
            break;
        }
    }
}
