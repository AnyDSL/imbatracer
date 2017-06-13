#ifndef IMBA_INTEGRATOR_H
#define IMBA_INTEGRATOR_H

#include "imbatracer/render/scheduling/ray_queue.h"
#include "imbatracer/render/ray_gen/camera.h"
#include "imbatracer/render/light.h"
#include "imbatracer/render/random.h"
#include "imbatracer/render/scene.h"

#include "imbatracer/core/mesh.h"
#include "imbatracer/core/image.h"
#include "imbatracer/core/rgb.h"

#include <functional>

namespace imba {

/// Base class for all integrators.
class Integrator {
public:
    Integrator(const Scene& scene, const PerspectiveCamera& cam)
        : scene_(scene), cam_(cam)
    {}

    virtual ~Integrator() {}

    /// Renders a frame, using the resolution and sample count specified in the camera.
    virtual void render(AtomicImage& out) = 0;

    /// Called whenever the camera view is updated.
    virtual void reset() {}

    /// Called once per scene at the beginning, before the other methods.
    virtual void preprocess() { estimate_pixel_size(); }

    /// Estimate of the average distance between hit points of rays from the same pixel.
    /// The value is computed during the preprocessing phase.
    /// The result of calling this function before preprocess() is undefined.
    float pixel_size() const { return pixel_size_; }

    /// Allows integrators to react on user input (e.g. for debugging)
    /// \returns true if the image should be reset.
    virtual bool key_press(int32_t k) { return false; }

protected:
    const Scene& scene_;
    const PerspectiveCamera& cam_;

    inline static void add_contribution(AtomicImage& out, int pixel_id, const rgb& contrib) {
        out.pixels()[pixel_id].apply<std::plus<float> >(contrib);
    }

    inline void process_shadow_rays(RayQueue<ShadowState>& ray_in, AtomicImage& out) {
        ShadowState* states = ray_in.states();
        Hit* hits = ray_in.hits();

        tbb::parallel_for(tbb::blocked_range<int>(0, ray_in.size()),
            [&] (const tbb::blocked_range<int>& range)
        {
            for (auto i = range.begin(); i != range.end(); ++i) {
                if (hits[i].tri_id < 0) {
                    // Nothing was hit, the light source is visible.
                    add_contribution(out, states[i].pixel_id, states[i].throughput);
                }
            }
        });
    }

private:
    float pixel_size_;

    void estimate_pixel_size();
};

inline Intersection calculate_intersection(const Scene& scene, const Hit& hit, const Ray& ray) {
    const Mesh::Instance& inst = scene.instance(hit.inst_id);
    const Mesh& mesh = scene.mesh(inst.id);

    const int local_tri_id = scene.local_tri_id(hit.tri_id, inst.id);

    const int i0 = mesh.indices()[local_tri_id * 4 + 0];
    const int i1 = mesh.indices()[local_tri_id * 4 + 1];
    const int i2 = mesh.indices()[local_tri_id * 4 + 2];
    const int  m = mesh.indices()[local_tri_id * 4 + 3];

    const auto& mat = scene.material(m);

    const float3     org(ray.org.x, ray.org.y, ray.org.z);
    const float3 out_dir(ray.dir.x, ray.dir.y, ray.dir.z);
    const auto       pos = org + hit.tmax * out_dir;
    const auto local_pos = inst.inv_mat * float4(pos, 1.0f);

    // Recompute v based on u and local_pos
    const float u = hit.u;
    const auto v0 = float3(mesh.vertices()[i0]);
    const auto e1 = float3(mesh.vertices()[i1]) - v0;
    const auto e2 = float3(mesh.vertices()[i2]) - v0;
    const float v = dot(local_pos - v0 - u * e1, e2) / dot(e2, e2);

    const auto texcoords    = mesh.attribute<float2>(MeshAttributes::TEXCOORDS);
    const auto normals      = mesh.attribute<float3>(MeshAttributes::NORMALS);
    const auto geom_normals = mesh.attribute<float3>(MeshAttributes::GEOM_NORMALS);

    const auto uv_coords    = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
    const auto local_normal = lerp(normals[i0], normals[i1], normals[i2], u, v);
    const auto normal       = normalize(float3(local_normal * inst.inv_mat));
    const auto geom_normal  = normalize(float3(geom_normals[local_tri_id] * inst.inv_mat));

    const auto w_out = -normalize(out_dir);

    float3 u_tangent;
    float3 v_tangent;
    local_coordinates(normal, u_tangent, v_tangent);

    Intersection res {
        pos, w_out, normal, uv_coords, geom_normal, u_tangent, v_tangent, mat.get()
    };

    // If the material has a bump map, modify the shading normal accordingly.
    mat->bump(res);

    // Ensure that the shading normal is always in the same hemisphere as the geometric normal.
    if (dot(res.geom_normal, res.normal) < 0.0f)
        res.normal = -res.normal;

    return res;
}

template<typename StateType>
void terminate_path(StateType& state) {
    state.pixel_id = -1;
}

} // namespace imba

#endif // IMBA_INTEGRATOR_H
