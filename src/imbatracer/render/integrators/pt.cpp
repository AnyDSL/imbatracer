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
        printf("hit at %d: %d \n", i, hits[i].tri_id);
        if (hits[i].tri_id != -1) {             
            auto& mat = scene_.materials[scene_.material_ids[hits[i].tri_id]];
                    
            float3 pos = float3(rays[i].org.x, rays[i].org.y, rays[i].org.z);
            float3 out_dir = float3(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
            pos = pos + (hits[i].tmax) * out_dir;
            
            float u = hits[i].u;
            float v = hits[i].v;
            float2 uv_coords = scene_.mesh.interpolate_attribute<float2>(MeshAttributes::texcoords, hits[i].tri_id, u, v);
            float3 normal = scene_.mesh.interpolate_attribute<float3>(MeshAttributes::normals, hits[i].tri_id, u, v);
            
            SurfaceInfo surf_info { normal, uv_coords.x, uv_coords.y };
            
            if (mat->kind == Material::emissive) {
                // If an emissive object is hit after a specular bounce or as the first intersection along the path, add its contribution. 
                // otherwise the light has to be ignored because it was already sampled as direct illumination.
                if (shader_state[i].bounces == 0) {                        
                    EmissiveMaterial* em = static_cast<EmissiveMaterial*>(mat.get());
                    float4 color = em->color();

                    // Add contribution to the pixel which this ray belongs to.
                    out.pixels()[shader_state[i].pixel_id * 4] += color.x;
                    out.pixels()[shader_state[i].pixel_id * 4 + 1] += color.y;
                    out.pixels()[shader_state[i].pixel_id * 4 + 2] += color.z;
                } else if (shader_state[i].last_specular) {
                    EmissiveMaterial* em = static_cast<EmissiveMaterial*>(mat.get());
                    
                    float cos_light = fabsf(dot(normal, -1.0f * out_dir));
                    if (cos_light > 0.0f && cos_light < 1.0f) {  // Only add contribution from the side of the light that the normal is on.
                        float4 color = shader_state[i].throughput * em->color();
                        
                        // Add contribution to the pixel which this ray belongs to.
                        out.pixels()[shader_state[i].pixel_id * 4] += color.x;
                        out.pixels()[shader_state[i].pixel_id * 4 + 1] += color.y;
                        out.pixels()[shader_state[i].pixel_id * 4 + 2] += color.z;
                    }
                }

                // Do not continue the path after hitting a light source.
                break;
                
            } else if (!mat.get()->is_specular) {
                // Compute direct illumination only for materials that are not specular.
                // Generate the shadow ray (sample one point on one lightsource)
                auto ls = scene_.lights[rng.random(0, scene_.lights.size() - 1)].get();
                auto sample = ls->sample(pos, rng.random01(), rng.random01());
                float3 sh_dir = sample.dir;
                float pdf = scene_.lights.size();
                
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
                
                // Compute the values stored in the ray state.
                float cos_term = fabsf(dot(sample.dir, normal));
                float4 brdf = evaluate_material(mat.get(), out_dir, surf_info, sample.dir);
                
                // Update the current state of this path.
                PTState s = shader_state[i];
                s.throughput = s.throughput * brdf * cos_term * sample.intensity * pdf;
                
                // Push the shadow ray into the queue.
                ray_out_shadow.push(ray, s);
            }
            
            // Continue the path using russian roulette.
            float rrprob = 0.7f;
            float u_rr = rng.random01();
            if (u_rr < rrprob) {
                // sample brdf
                float pdf;
                float3 sample_dir;
                float4 brdf = sample_material(mat.get(), out_dir, surf_info, rng.random01(), rng.random01(), sample_dir, pdf);
                
                float cos_term = fabsf(dot(normal, sample_dir));
                
                PTState s = shader_state[i];
                s.throughput = s.throughput * brdf * (cos_term / (rrprob * pdf));
                s.bounces++;
                s.last_specular = mat.get()->is_specular;
                
                Ray ray;
                ray.org.x = pos.x;
                ray.org.y = pos.y;
                ray.org.z = pos.z;
                ray.org.w = 0.001;
                
                ray.dir.x = sample_dir.x;
                ray.dir.y = sample_dir.y;
                ray.dir.z = sample_dir.z;
                ray.dir.w = FLT_MAX;
                
                ray_out.push(ray, s);
            }
        }
    }
}

void PathTracer::process_shadow_rays(RayQueue<PTState>& ray_in, Image& out) {
    PTState* shader_state = ray_in.states();
    Hit* hits = ray_in.hits(); 
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size(); 
    
    for (int i = 0; i < ray_count; ++i) {
        float4 color = float4(0.0f);
                
        if (hits[i].tri_id == -1) {
            // The shadow ray hit the light source. Multiply the contribution of the light by the 
            // current throughput of the path (as stored in the state of the shadow ray)
            color = shader_state[i].throughput;
        }
    
        // Add contribution to the pixel which this ray belongs to.
        out.pixels()[shader_state[i].pixel_id * 4] += color.x;
        out.pixels()[shader_state[i].pixel_id * 4 + 1] += color.y;
        out.pixels()[shader_state[i].pixel_id * 4 + 2] += color.z;
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
        
        printf("traverse %d rays \n", primary_rays[in_queue].size());
        
        primary_rays[in_queue].traverse();
        process_primary_rays(primary_rays[in_queue], primary_rays[out_queue], shadow_rays, out);
        printf("traverse %d rays \n", shadow_rays.size());
        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays.size() > 0) {
            shadow_rays.traverse();
            process_shadow_rays(shadow_rays, out);
        }
        
        std::swap(in_queue, out_queue);
    }
}

} // namespace imba