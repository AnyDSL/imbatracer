#include "pt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <random>

namespace imba {

void PathTracer::process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, Image& out) {
    static RNG rng;
    
    PTState* shader_state = ray_in.states();
    Hit* hits = ray_in.hits(); 
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size(); 
    
    for (int i = 0; i < ray_count; ++i) {       
        if (hits[i].tri_id < 0)
            continue;             

        auto& mat = scene_.materials[scene_.material_ids[hits[i].tri_id]];
                
        const float3 org(rays[i].org.x, rays[i].org.y, rays[i].org.z);
        const float3 out_dir(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
        const float3 pos = org + (hits[i].tmax) * out_dir;
        
        const float u = hits[i].u;
        const float v = hits[i].v;
        const int i0 = scene_.mesh.indices()[hits[i].tri_id * 3 + 0];
        const int i1 = scene_.mesh.indices()[hits[i].tri_id * 3 + 1];
        const int i2 = scene_.mesh.indices()[hits[i].tri_id * 3 + 2];

        const auto texcoords = scene_.mesh.attribute<float2>(MeshAttributes::texcoords);
        const auto normals = scene_.mesh.attribute<float3>(MeshAttributes::normals);

        const float2 uv_coords = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
        const float3 normal = lerp(normals[i0], normals[i1], normals[i2], u, v);
        
        SurfaceInfo surf_info { normal, uv_coords.x, uv_coords.y };
        
        if (mat->kind == Material::emissive) {
            // If an emissive object is hit after a specular bounce or as the first intersection along the path, add its contribution. 
            // otherwise the light has to be ignored because it was already sampled as direct illumination.
            if (shader_state[i].bounces == 0) {                        
                EmissiveMaterial* em = static_cast<EmissiveMaterial*>(mat.get());
                float4 color = em->color();

                // Add contribution to the pixel which this ray belongs to.
                out.pixels()[shader_state[i].pixel_id] += color;
            } else if (shader_state[i].last_specular) {
                EmissiveMaterial* em = static_cast<EmissiveMaterial*>(mat.get());
                
                float cos_light = fabsf(dot(normal, -1.0f * out_dir));
                if (cos_light > 0.0f && cos_light < 1.0f) {  // Only add contribution from the side of the light that the normal is on.
                    float4 color = shader_state[i].throughput * em->color();
                    
                    // Add contribution to the pixel which this ray belongs to.
                    out.pixels()[shader_state[i].pixel_id] += color;
                }
            }

            // Do not continue the path after hitting a light source.
            continue;
        } 
        
        // Compute direct illumination only for materials that are not specular.
        // Generate the shadow ray (sample one point on one lightsource)
        const auto ls = scene_.lights[rng.random(0, scene_.lights.size() - 1)].get();
        const auto sample = ls->sample(pos, rng.random01(), rng.random01());
        const float3 sh_dir = sample.dir;
        const float pdf = scene_.lights.size();
        
        Ray ray {
            { pos.x, pos.y, pos.z, 0.01f },
            { sh_dir.x, sh_dir.y, sh_dir.z, sample.distance - 0.01f }
        };
        
        // Compute the values stored in the ray state.
        const float cos_term = fabsf(dot(sample.dir, normal));
        const float4 brdf = evaluate_material(mat.get(), out_dir, surf_info, sample.dir);
        
        // Update the current state of this path.
        PTState s = shader_state[i];
        s.throughput = s.throughput * brdf * cos_term * sample.intensity * pdf;
        
        // Push the shadow ray into the queue.
        ray_out_shadow.push(ray, s);
      
        // Continue the path using russian roulette.
        const float4 srgb(0.2126, 0.7152, 0.0722, 0.0f);
        const float kill_prob = dot(shader_state[i].throughput, srgb) * 100.0f;

        const float rrprob = std::min(1.0f, kill_prob);
        const float u_rr = rng.random01();
        const int max_recursion = 32; // prevent havoc
        if (u_rr < rrprob && shader_state[i].bounces < max_recursion) {
            // sample brdf
            float pdf;
            float3 sample_dir;
            bool specular;

            const float4 brdf = sample_material(mat.get(), out_dir, surf_info, rng, sample_dir, pdf, specular);
            const float cos_term = fabsf(dot(normal, sample_dir));
            
            PTState s = shader_state[i];
            s.throughput = s.throughput * brdf * (cos_term / (rrprob * pdf));
            s.bounces++;
            s.last_specular = specular;
            
            Ray ray {
                { pos.x, pos.y, pos.z, 0.01f },
                { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
            };
            
            ray_out.push(ray, s);
        }
    }
}

void PathTracer::process_shadow_rays(RayQueue<PTState>& ray_in, Image& out) {
    PTState* shader_state = ray_in.states();
    Hit* hits = ray_in.hits(); 
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size(); 

    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0) {
            // The shadow ray hit the light source. Multiply the contribution of the light by the 
            // current throughput of the path (as stored in the state of the shadow ray)
            float4 color = shader_state[i].throughput;
            // Add contribution to the pixel which this ray belongs to.
            out.pixels()[shader_state[i].pixel_id] += color;
        }        
    }
}

void PathTracer::render(Image& out) {    
    // Create the initial set of camera rays.
    auto camera = static_cast<PixelRayGen<PTState>*>(scene_.camera);
    camera->start_frame();
    
    int in_queue = 0;
    int out_queue = 1;
    
    while(true) {
        camera->fill_queue(primary_rays[in_queue]);
        
        if (primary_rays[in_queue].size() <= 0)
            break;

        primary_rays[in_queue].traverse();
        process_primary_rays(primary_rays[in_queue], primary_rays[out_queue], shadow_rays, out);
        primary_rays[in_queue].clear();

        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays.size() > 0) {
            shadow_rays.traverse_occluded();
            process_shadow_rays(shadow_rays, out);
            shadow_rays.clear();
        }

        std::swap(in_queue, out_queue);
    }
}

} // namespace imba
