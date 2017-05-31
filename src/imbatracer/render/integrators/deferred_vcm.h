#ifndef IMBA_DEFERRED_VCM_H
#define IMBA_DEFERRED_VCM_H

#include "imbatracer/render/scheduling/deferred_scheduler.h"

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/integrators/deferred_vertices.h"
#include "imbatracer/render/integrators/deferred_mis.h"

#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/frontend/cmd_line.h"

namespace imba {

class DeferredVCM : public Integrator {
    struct State : public RayState {
        /// The power or importance carried by the path up to this intersection
        rgb throughput;

        /// Number of vertices along this path until now (includes vertex at camera / light)
        int path_length;

        /// Index within the vertex cache where the previous vertex along this path was stored
        /// -1 if the ancestor was not stored (e.g. is on a specular surface, first vertex, cache too small, ...)
        int ancestor;

        PartialMIS mis;
    };

    struct ShadowState : public RayState {
        /// Weighted contribution of the shadow ray if it is not occluded
        rgb contrib;
    };

    struct Vertex {
        PartialMIS mis;
        rgb contrib; ///< The power or importance of the path leading to this vertex
        int ancestor;

        Vertex() {}
        Vertex(const PartialMIS& mis, const rgb& c, int a) : mis(mis), contrib(c), ancestor(a) {}
    };

    using VertCache = DeferredVertices<Vertex>;

public:
    DeferredVCM(Scene& scene, PerspectiveCamera& cam, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , cur_iteration_(0)
        , light_tile_gen_(scene.light_count(), settings.light_path_count, settings.tile_size * settings.tile_size)
        , camera_tile_gen_(settings.width, settings.height, settings.concurrent_spp, settings.tile_size)
        , scheduler_(&scene_, settings.thread_count, settings.q_size,
                     settings.traversal_platform == UserSettings::gpu,
                     std::max(camera_tile_gen_.sizeof_ray_gen(), light_tile_gen_.sizeof_ray_gen()))
        //, shadow_scheduler_() TODO
    {
        // Compute the required cache size for storing the light and camera vertices.
        bool use_gpu = settings.traversal_platform == UserSettings::gpu;
        int avg_light_v = estimate_light_path_len(scene, use_gpu, 10000);
        int avg_cam_v = estimate_cam_path_len(scene, cam, use_gpu, 1);
        int num_cam_v   = 1.1f * avg_cam_v * settings.width * settings.height * settings.concurrent_spp;
        int num_light_v = 1.1f * avg_light_v * settings.light_path_count;

        cam_verts_.reset(new VertCache(num_cam_v));
        light_verts_.reset(new VertCache(num_light_v));
    }

    void render(AtomicImage& img) override final;

    void reset() override final {
        pm_radius_ = base_radius_;
        cur_iteration_ = 0;
    }

    void preprocess() override final {
        Integrator::preprocess();
        base_radius_ = pixel_size() * settings_.radius_factor;
    }

private:
    UserSettings settings_;

    int cur_iteration_;
    float pm_radius_;
    float base_radius_;

    UniformLightTileGen<State> light_tile_gen_;
    DefaultTileGen<State> camera_tile_gen_;

    DeferredScheduler<State> scheduler_;
    //DeferredScheduler<ShadowState> shadow_scheduler_;

    std::unique_ptr<VertCache> cam_verts_;
    std::unique_ptr<VertCache> light_verts_;

    void trace_camera_paths();
    void trace_light_paths();
    void process_hits(Ray& r, Hit& h, State& s, VertCache* cache);
    void process_envmap_hits(Ray& r, State& s);

    void bounce(State& state_out, const Intersection& isect, BSDF* bsdf, Ray& ray_out, bool adjoint, float offset);

    /// Computes the cosine term for adjoint BSDFs that use shading normals.
    ///
    /// This function has to be used for all BSDFs while tracing paths from the light sources, to prevent brighness discontinuities.
    /// See Veach's thesis for more details.
    inline static float shading_normal_adjoint(const float3& normal, const float3& geom_normal, const float3& out_dir, const float3& in_dir) {
        return dot(out_dir, normal) * dot(in_dir, geom_normal) / dot(out_dir, geom_normal);
    }

    // Sampling techniques (additional to camera rays hitting the light)
    void direct_illum(AtomicImage& img);
    void connect_to_camera(AtomicImage& img);
    void connect(AtomicImage& img);
    void merge(AtomicImage& img);
};

} // namespace imba

#endif // IMBA_DEFERRED_VCM_H