#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "../ray_queue.h"
#include "../camera.h"
#include "../../core/image.h"
#include "../light.h"
#include "../random.h"
#include "../scene.h"

#include "../../core/mesh.h"

namespace imba {

class Integrator {
public:
    Integrator(Scene& scene) : scene_(scene) {}

    virtual void render(Image& out) = 0;
    
protected:
    struct Intersection {
        SurfaceInfo surf;
        float3 pos;
        float3 out_dir;  
        Material* mat;
    };
    
    Scene& scene_;
    
    inline Intersection calculate_intersection(const Hit* const hits, const Ray* const rays, const int i) {
        const int i0 = scene_.mesh.indices()[hits[i].tri_id * 4 + 0];
        const int i1 = scene_.mesh.indices()[hits[i].tri_id * 4 + 1];
        const int i2 = scene_.mesh.indices()[hits[i].tri_id * 4 + 2];
        const int  m = scene_.mesh.indices()[hits[i].tri_id * 4 + 3];

        const auto& mat = scene_.materials[m];
                
        const float3     org(rays[i].org.x, rays[i].org.y, rays[i].org.z);
        const float3 out_dir(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
        const float3 pos = org + (hits[i].tmax) * out_dir;
        
        const float u = hits[i].u;
        const float v = hits[i].v;

        const auto texcoords = scene_.mesh.attribute<float2>(MeshAttributes::texcoords);
        const auto normals = scene_.mesh.attribute<float3>(MeshAttributes::normals);

        const float2 uv_coords = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
        const float3 normal = normalize(lerp(normals[i0], normals[i1], normals[i2], u, v));

        SurfaceInfo surf_info { normal, uv_coords, float3() };
        
        return {
            SurfaceInfo { normal, uv_coords.x, uv_coords.y },
            pos, -out_dir, mat.get()
        };
    }

    template<typename StateType>
    inline void compute_direct_illum(RNG& rng, const Intersection& isect, const StateType& state, RayQueue<StateType>& ray_out_shadow) {
        float offset = 0.0001f;

        // Generate the shadow ray (sample one point on one lightsource)
        const auto ls = scene_.lights[rng.random(0, scene_.lights.size() - 1)].get();
        const auto sample = ls->sample(isect.pos, rng.random01(), rng.random01());
        const float3 sh_dir = normalize(sample.dir);
        const float pdf = scene_.lights.size();

        // Ensure that the incoming and outgoing directions are on the same side of the surface.
        const float cos_sign = dot(sample.dir, isect.surf.normal);
        if (cos_sign * dot(isect.out_dir, isect.surf.normal) < 0.0f)
            return;

        Ray ray {
            { isect.pos.x, isect.pos.y, isect.pos.z, offset },
            { sh_dir.x, sh_dir.y, sh_dir.z, sample.distance - offset }
        };
        
        // Compute the values stored in the ray state.
        const float cos_term = fabsf(cos_sign);
        const float4 brdf = evaluate_material(isect.mat, isect.out_dir, isect.surf, sample.dir);

        // Update the current state of this path.
        StateType s = state;
        s.throughput = s.throughput * brdf * cos_term * sample.intensity * pdf;
        
        // Push the shadow ray into the queue.
        ray_out_shadow.push(ray, s);
    }
};

}

#endif
