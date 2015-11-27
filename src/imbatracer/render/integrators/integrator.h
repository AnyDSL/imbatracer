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
        
        const float3 geometry_normal = scene_.geometry_normals[hits[i].tri_id];
        
        return {
            SurfaceInfo { normal, geometry_normal, uv_coords.x, uv_coords.y },
            pos, out_dir, mat.get()
        };
    }
};

}

#endif
