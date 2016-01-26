#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "../ray_queue.h"
#include "../ray_gen.h"
#include "../camera.h"
#include "../light.h"
#include "../random.h"
#include "../scene.h"

#include "../../core/mesh.h"
#include "../../core/image.h"

namespace imba {

class Integrator {
public:
    Integrator(Scene& scene, PerspectiveCamera& cam, int n_samples)
        : scene_(scene), cam_(cam), n_samples_(n_samples)
    {}

    virtual void render(Image& out) = 0;

protected:
    Scene& scene_;
    PerspectiveCamera& cam_;
    int n_samples_;

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
        auto geom_normal = scene_.geom_normals[hits[i].tri_id];

        const float2 uv_coords = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
        float3          normal = normalize(lerp(normals[i0], normals[i1], normals[i2], u, v));

        const float3 w_out = -normalize(out_dir);

        if (isinf(w_out.x))
            printf("gotchabala (%f,%f,%f) (%f,%f,%f)\n", w_out.x, w_out.y, w_out.z, out_dir.x, out_dir.y, out_dir.z);

        return {
            pos, w_out, hits[i].tmax, normal, uv_coords, geom_normal, mat.get()
        };
    }

    template<typename StateType>
    inline void compute_direct_illum(RNG& rng, const Intersection& isect, const StateType& state, RayQueue<StateType>& ray_out_shadow) {
        const float offset = 0.001f;

        // Generate the shadow ray (sample one point on one lightsource)
        const auto ls = scene_.lights[rng.random_int(0, scene_.lights.size())].get();
        const float pdf = scene_.lights.size();
        const auto sample = ls->sample_direct(isect.pos, rng);
        assert_normalized(sample.dir);

        // Ensure that the incoming and outgoing directions are on the same side of the surface.
        if (dot(isect.geom_normal, sample.dir) *
            dot(isect.geom_normal, isect.out_dir) <= 0.0f)
            return;

        Ray ray {
            { isect.pos.x, isect.pos.y, isect.pos.z, offset },
            { sample.dir.x, sample.dir.y, sample.dir.z, sample.distance - offset }
        };

        // Compute the values stored in the ray state.
        float pdf_dir, pdf_rev;
        const float4 brdf = evaluate_material(isect.mat, isect, sample.dir, false, pdf_dir, pdf_rev);

        // Update the current state of this path.
        StateType s = state;
        s.throughput *= brdf * sample.radiance * pdf;

        // Push the shadow ray into the queue.
        ray_out_shadow.push(ray, s);
    }
};

}

#endif
