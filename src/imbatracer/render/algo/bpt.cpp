#include "../integrator.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>

namespace imba {

void BidirPathTracer::shade_light_rays(RayQueue<BPTState>& ray_in, Image& out, RayQueue<BPTState>& ray_out) {
    static RNG rng;

    int ray_count = ray_in.size(); 
    const BPTState* states = ray_in.states();
    const Hit* hits = ray_in.hits(); 
    const Ray* rays = ray_in.rays();
    for (int i = 0; i < ray_count; ++i) { 
        if (hits[i].tri_id != -1) {
            float3 pos = float3(rays[i].org.x, rays[i].org.y, rays[i].org.z);
            float3 ray_dir = float3(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
            pos = pos + hits[i].tmax * ray_dir;
            float3 normal = normals_[hits[i].tri_id];
            auto& mat = materials_[material_ids_[hits[i].tri_id]];
        
            // Create a new vertex for this light path.
            int vertex_id = (light_path_lengths_[states[i].pixel_id][states[i].sample_id])++;
            
            auto& light_vertex = light_paths_[states[i].pixel_id][states[i].sample_id][vertex_id];
            light_vertex.pos = pos;
            light_vertex.light_id = states[i].light_id;
            light_vertex.is_specular = mat->is_specular;
           
            // Decide wether or not to continue this path.
            if (vertex_id < max_light_path_length - 1) {
                float pdf;
                float3 sample_dir;
                float4 brdf = sample_material(mat.get(), ray_dir, normal, rng.random01(), rng.random01(), sample_dir, pdf);
                float cos_term = fabsf(dot(normal, sample_dir));
                
                BPTState s = states[i];
                s.kind = RANDOM_RAY;
                s.throughput = s.throughput * brdf * (cos_term / pdf);
                s.bounces++;
                
                Ray ray;
                ray.org.x = pos.x;
                ray.org.y = pos.y;
                ray.org.z = pos.z;
                ray.org.w = 0.001;
                
                ray.dir.y = sample_dir.y;
                ray.dir.x = sample_dir.x;
                ray.dir.z = sample_dir.z;
                ray.dir.w = FLT_MAX;
                
                ray_out.push(ray, s);
            }
        }
    }
}

void BidirPathTracer::shade_camera_rays(RayQueue<BPTState>& ray_in, Image& out, RayQueue<BPTState>& ray_out) {
    static RNG rng;

    int ray_count = ray_in.size(); 
    const BPTState* states = ray_in.states();
    const Hit* hits = ray_in.hits(); 
    const Ray* rays = ray_in.rays();
    for (int i = 0; i < ray_count; ++i) { 
        switch (states[i].kind) {
        case CAMERA_RAY:
        case RANDOM_RAY:
            if (hits[i].tri_id != -1) {
                float3 pos = float3(rays[i].org.x, rays[i].org.y, rays[i].org.z);
                float3 ray_dir = float3(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
                pos = pos + hits[i].tmax * ray_dir;
                float3 normal = normals_[hits[i].tri_id];
                auto& mat = materials_[material_ids_[hits[i].tri_id]];
                
                int n_vertices = light_path_lengths_[states[i].pixel_id][states[i].sample_id];
                auto& sample_path = light_paths_[states[i].pixel_id][states[i].sample_id];
                
                // Connect the hitpoint to the light path.
                BPTState s = states[i];
                s.kind = SHADOW_RAY;
                
                float3 connect_dir = sample_path[0].pos - pos;
                
                Ray ray;
                ray.org.x = pos.x;
                ray.org.y = pos.y;
                ray.org.z = pos.z;
                ray.org.w = 0.001;
                
                ray.dir.y = connect_dir.y;
                ray.dir.x = connect_dir.x;
                ray.dir.z = connect_dir.z;
                ray.dir.w = FLT_MAX;
                
                ray_out.push(ray, s);
            }
            break;
            
        case SHADOW_RAY:
            float4 color;
            out.pixels()[states[i].pixel_id * 4] += color.x;
            out.pixels()[states[i].pixel_id * 4 + 1] += color.y;
            out.pixels()[states[i].pixel_id * 4 + 2] += color.z;
        }
    }
}

} // namespace imba