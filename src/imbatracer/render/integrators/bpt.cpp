#include "bpt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>

namespace imba {

void BidirPathTracer::process_light_rays(RayQueue<LightRayState>& rays_in, RayQueue<LightRayState>& rays_out) {
    static RNG rng;

    int ray_count = rays_in.size(); 
    const LightRayState* states = rays_in.states();
    const Hit* hits = rays_in.hits(); 
    const Ray* rays = rays_in.rays();
    
    for (int i = 0; i < ray_count; ++i) { 
        if (hits[i].tri_id < 0)
            continue;
        
        Intersection isect = calculate_intersection(hits, rays, i);
        
        // Create a new vertex for this light path.
        int vertex_id = (light_path_lengths_[states[i].pixel_id][states[i].sample_id])++;
        
        auto& light_vertex = light_paths_[states[i].pixel_id][states[i].sample_id][vertex_id];
        light_vertex.pos = isect.pos;
        light_vertex.light_id = states[i].light_id;
        
        // Decide wether or not to continue this path.
        if (vertex_id < MAX_LIGHT_PATH_LEN - 1) {
            float pdf;
            float3 sample_dir;
            bool is_specular;
            float4 brdf = sample_material(isect.mat, isect.out_dir, isect.surf, rng, sample_dir, pdf, is_specular);
            
            // As we are using shading normals, we need to use the adjoint BSDF
            float cos_out_snorm = fabsf(dot(isect.surf.normal, isect.out_dir));
            float cos_out_gnorm = fabsf(dot(isect.surf.geom_normal, isect.out_dir));
            float cos_in_gnorm = fabsf(dot(isect.surf.geom_normal, sample_dir));
            float adjoint_conversion = cos_out_snorm / cos_out_gnorm * cos_in_gnorm;
            
            LightRayState s = states[i];
            s.throughput = s.throughput * brdf * adjoint_conversion / pdf;
            s.bounces++;
            
            // Store the throughput etc. in the light path vertex. 
            light_vertex.is_specular = is_specular;
            light_vertex.throughput = s.throughput;
            light_vertex.power = states[i].throughput * states[i].power;
            
            Ray ray {
                { isect.pos.x, isect.pos.y, isect.pos.z, 0.01f },
                { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
            };
            
            rays_out.push(ray, s);
        }
    }
}

void BidirPathTracer::process_primary_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& ray_out_shadow, Image& img) {
    static RNG rng;

    const float offset = 0.0001f;

    int ray_count = rays_in.size(); 
    const BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits(); 
    const Ray* rays = rays_in.rays();
    
    for (int i = 0; i < ray_count; ++i) {        
        if (hits[i].tri_id < 0) 
            continue;
            
        Intersection isect = calculate_intersection(hits, rays, i);
        
        int n_vertices = light_path_lengths_[states[i].pixel_id][states[i].sample_id];
        auto& sample_path = light_paths_[states[i].pixel_id][states[i].sample_id];
       
        compute_direct_illum(rng, isect, states[i], ray_out_shadow);
        
        // Connect the hitpoint to the light path.
        for (int i = 0; i < n_vertices; ++i) {
            float3 dir = sample_path[i].pos - isect.pos;
            const float distance = length(dir);
            dir = dir / distance;
                        
            const float cos_term = fabsf(dot(dir, isect.surf.normal));
            const float4 brdf = evaluate_material(isect.mat, isect.out_dir, isect.surf, dir);
            const float4 throughput = states[i].throughput * brdf * cos_term;
            
            BPTState s = states[i];
            s.contribution = sample_path[i].power * throughput;
            
            Ray ray {
                { isect.pos.x, isect.pos.y, isect.pos.z, 0.01f },
                { dir.x, dir.y, dir.z, distance - 0.01f }
            };
            
            shadow_rays_.push(ray, s);
        }
        
        // continue the camera path
        const float4 srgb(0.2126f, 0.7152f, 0.0722f, 0.0f);
        const float kill_prob = dot(states[i].throughput, srgb) * 100.0f;

        const float rrprob = std::min(1.0f, kill_prob);
        const float u_rr = rng.random_float();
        const int max_recursion = 32; // prevent havoc
        if (u_rr < rrprob && states[i].bounces < max_recursion) {
            // sample brdf
            float pdf;
            float3 sample_dir;
            bool specular;

            const float4 brdf = sample_material(isect.mat, isect.out_dir, isect.surf, rng, sample_dir, pdf, specular);
            const float cos_term = fabsf(dot(isect.surf.normal, sample_dir));
            
            BPTState s = states[i];
            s.throughput = s.throughput * brdf * (cos_term / (rrprob * pdf));

            s.bounces++;
            s.last_specular = specular;

            Ray ray {
                { isect.pos.x, isect.pos.y, isect.pos.z, offset },
                { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
            };

            rays_out.push(ray, s);
        }
    }
}

void BidirPathTracer::process_shadow_rays(RayQueue<BPTState>& rays_in, Image& img) {
    int ray_count = rays_in.size(); 
    const BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits(); 
    const Ray* rays = rays_in.rays();
    
    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id >= 0) {
            img.pixels()[states[i].pixel_id] += states[i].contribution;
        }
    }
}

void BidirPathTracer::trace_light_paths() {
    light_sampler_.start_frame();
    
    int in_queue = 0;
    int out_queue = 1;
    
    while(true) {
        light_sampler_.fill_queue(light_rays_[in_queue]);
        
        if (light_rays_[in_queue].size() <= 0)
            break;
        
        light_rays_[in_queue].traverse(scene_);
        process_light_rays(light_rays_[in_queue], light_rays_[out_queue]);
        light_rays_[in_queue].clear();
        
        std::swap(in_queue, out_queue);
    }
}

void BidirPathTracer::trace_camera_paths(Image& img) {
    // Create the initial set of camera rays.
    auto camera = static_cast<PixelRayGen<BPTState>*>(scene_.camera);
    camera->start_frame();
    
    int in_queue = 0;
    int out_queue = 1;
    
    while(true) {
        camera->fill_queue(primary_rays_[in_queue]);
         
        if (primary_rays_[in_queue].size() <= 0)
            break;

        primary_rays_[in_queue].traverse(scene_);
        process_primary_rays(primary_rays_[in_queue], primary_rays_[out_queue], shadow_rays_, img);
        primary_rays_[in_queue].clear();

        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays_.size() > 0) {
            shadow_rays_.traverse_occluded(scene_);
            process_shadow_rays(shadow_rays_, img);
            shadow_rays_.clear();
        }

        std::swap(in_queue, out_queue);
    }
}

void BidirPathTracer::reset_light_paths() {
    for (auto& p : light_path_lengths_) {
        std::fill(p.begin(), p.end(), 0);
    }
}

void BidirPathTracer::render(Image& img) {
    reset_light_paths();
        
    trace_light_paths();
    trace_camera_paths(img);
}

} // namespace imba
