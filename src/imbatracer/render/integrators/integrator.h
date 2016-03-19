#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "../ray_queue.h"
#include "../camera.h"
#include "../light.h"
#include "../random.h"
#include "../scene.h"

#include "../../core/mesh.h"
#include "../../core/image.h"

namespace imba {

class Integrator {
public:
    Integrator(Scene& scene, PerspectiveCamera& cam)
        : scene_(scene), cam_(cam)
    {}

    virtual void render(AtomicImage& out) = 0;
    virtual void reset() {}

protected:
    Scene& scene_;
    PerspectiveCamera& cam_;

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

        const auto texcoords   = scene_.mesh.attribute<float2>(MeshAttributes::texcoords);
        const auto normals     = scene_.mesh.attribute<float3>(MeshAttributes::normals);
        const auto geom_normal = scene_.geom_normals[hits[i].tri_id];

        const float2 uv_coords = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
        const float3    normal = normalize(lerp(normals[i0], normals[i1], normals[i2], u, v));

        const float3 w_out = -normalize(out_dir);

        // Compute the tangents according to uv parameterization. (Used during bump mapping)
        // Based on PBRT p. 143
        float3 u_tangent;
        float3 v_tangent;

        const float du1 = texcoords[i0][0] - texcoords[i2][0];
        const float du2 = texcoords[i1][0] - texcoords[i2][0];
        const float dv1 = texcoords[i0][1] - texcoords[i2][1];
        const float dv2 = texcoords[i1][1] - texcoords[i2][1];

        const float4 dp1 = scene_.mesh.vertices()[i0] - scene_.mesh.vertices()[i2];
        const float4 dp2 = scene_.mesh.vertices()[i1] - scene_.mesh.vertices()[i2];

        const float determinant = du1 * dv2 - dv1 * du2;

        if (determinant == 0.0f)
            local_coordinates(geom_normal, u_tangent, v_tangent);
        else {
            const float inv_det = 1.0f / determinant;
            u_tangent = truncate(( dv2 * dp1 - dv1 * dp2) * inv_det);
            v_tangent = truncate((-du2 * dp1 + du1 * dp2) * inv_det);
        }

        Intersection res {
            pos, w_out, hits[i].tmax, normal, uv_coords, geom_normal, normalize(u_tangent), normalize(v_tangent), mat.get()
        };

        // If the material has a bump map, modify the shading normal accordingly.
        mat->bump(res);

        // Ensure that the shading normal is always in the same hemisphere as the geometric normal.
        if (dot(res.geom_normal, res.normal) < 0.0f)
            res.normal *= -1.0f;

        return res;
    }
};

}

#endif
