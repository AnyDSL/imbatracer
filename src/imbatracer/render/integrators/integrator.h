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
    Integrator(const Scene& scene, const PerspectiveCamera& cam)
        : scene_(scene), cam_(cam)
    {}

    virtual ~Integrator() {}

    virtual void render(AtomicImage& out) = 0;
    virtual void reset() {}
    virtual void preprocess() {}

protected:
    const Scene& scene_;
    const PerspectiveCamera& cam_;

    Intersection calculate_intersection(const Hit* const hits, const Ray* const rays, const int i) const {
        const Mesh::Instance& inst = scene_.instance(hits[i].inst_id);
        const Mesh& mesh = scene_.mesh(inst.id);

        const int i0 = mesh.indices()[hits[i].tri_id * 4 + 0];
        const int i1 = mesh.indices()[hits[i].tri_id * 4 + 1];
        const int i2 = mesh.indices()[hits[i].tri_id * 4 + 2];
        const int  m = mesh.indices()[hits[i].tri_id * 4 + 3];

        const auto& mat = scene_.material(m);

        const float3     org(rays[i].org.x, rays[i].org.y, rays[i].org.z);
        const float3 out_dir(rays[i].dir.x, rays[i].dir.y, rays[i].dir.z);
        const float3       pos(org + (hits[i].tmax) * out_dir);
        const float3 local_pos(inst.inv_mat * float4(pos, 1));

        // Recompute v based on u and local_pos
        const float u = hits[i].u;
        const float3 v0 = float3(mesh.vertices()[i0]);
        const float3 e1 = float3(mesh.vertices()[i1]) - v0;
        const float3 e2 = float3(mesh.vertices()[i2]) - v0;
        const float v = dot(local_pos - v0 - u * e1, e2) / dot(e2, e2);

        const auto texcoords    = mesh.attribute<float2>(MeshAttributes::TEXCOORDS);
        const auto normals      = mesh.attribute<float3>(MeshAttributes::NORMALS);
        const auto geom_normals = mesh.attribute<float3>(MeshAttributes::GEOM_NORMALS);

        const float2 uv_coords    = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
        const float4 local_normal = float4(lerp(normals[i0], normals[i1], normals[i2], u, v), 0.0f);
        const float3 normal       = normalize(float3(local_normal * inst.inv_mat));
        const float3 geom_normal  = normalize(float3(float4(geom_normals[hits[i].tri_id], 0) * inst.inv_mat));;

        const float3 w_out = -normalize(out_dir);

        float3 u_tangent;
        float3 v_tangent;
        local_coordinates(normal, u_tangent, v_tangent);

        Intersection res {
            pos, w_out, hits[i].tmax, normal, uv_coords, geom_normal, u_tangent, v_tangent, mat.get()
        };

        // If the material has a bump map, modify the shading normal accordingly.
        mat->bump(res);

        // Ensure that the shading normal is always in the same hemisphere as the geometric normal.
        if (dot(res.geom_normal, res.normal) < 0.0f)
            res.normal *= -1.0f;

        return res;
    }
};

} // namespace imba

#endif // IMBA_INTEGRATOR_H
