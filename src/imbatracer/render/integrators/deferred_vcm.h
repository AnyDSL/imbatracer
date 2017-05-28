#ifndef IMBA_DEFERRED_VCM_H
#define IMBA_DEFERRED_VCM_H

#include "imbatracer/render/scheduling/deferred_scheduler.h"

#include "imbatracer/render/integrators/integrator.h"

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

        // partial MIS weights
        float partial_unidir;
        float partial_connect;
        float partial_merge;

        /// pdf for sampling the current ray direction at the last intersection
        float last_pdf;
    };

    struct ShadowState : public RayState {
        /// Weighted contribution of the shadow ray if it is not occluded
        rgb contrib;
    };

    class ConnectTileGen : public TileGen<ShadowState> {
    public:
        using typename TileGen<ShadowState>::TilePtr;

        TilePtr next_tile(uint8_t*) override final {}

        size_t sizeof_ray_gen() const override final {}

        void start_frame() override final {}
    };

    class NextEventTileGen : public TileGen<ShadowState> {
    public:
        using typename TileGen<ShadowState>::TilePtr;

        TilePtr next_tile(uint8_t*) override final {}

        size_t sizeof_ray_gen() const override final {}

        void start_frame() override final {}
    };

    class CamConnectTileGen : public TileGen<ShadowState> {
    public:
        using typename TileGen<ShadowState>::TilePtr;

        TilePtr next_tile(uint8_t*) override final {}

        size_t sizeof_ray_gen() const override final {}

        void start_frame() override final {}
    };

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
    }

    void render(AtomicImage& out) override final;

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

    void trace_camera_paths(AtomicImage& out);
    void process_camera_hits(Ray& r, Hit& h, State& s, AtomicImage& img);
    void process_light_hits(Ray& r, Hit& h, State& s, AtomicImage& img);
    void process_camera_empties(Ray& r, State& s, AtomicImage& img);
    void trace_light_paths(AtomicImage& img);

    void bounce(State& state_out, const Intersection& isect, BSDF* bsdf, Ray& ray_out, bool adjoint, float offset);

    /// Computes the cosine term for adjoint BSDFs that use shading normals.
    ///
    /// This function has to be used for all BSDFs while tracing paths from the light sources, to prevent brighness discontinuities.
    /// See Veach's thesis for more details.
    inline static float shading_normal_adjoint(const float3& normal, const float3& geom_normal, const float3& out_dir, const float3& in_dir) {
        return dot(out_dir, normal) * dot(in_dir, geom_normal) / dot(out_dir, geom_normal);
    }

    // Sampling techniques (additional to camera rays hitting the light)
    void direct_illum(AtomicImage& out);
    void connect_to_camera(AtomicImage& out);
    void connect(AtomicImage& out);
    void merge(AtomicImage& out);

    // MIS weight computations
    inline void init_camera_mis(const Ray& r, State& s) {
        const float3 dir(r.dir.x, r.dir.y, r.dir.z);
        const float cos_theta_o = dot(dir, cam_.dir());

        // PDF on image plane is 1. We need to convert this from image plane area to solid angle.

        assert(cos_theta_o > 0.0f);
        const float pdf_cam_w = sqr(cam_.image_plane_dist() / cos_theta_o) / cos_theta_o;

        s.partial_connect = 1.0f;
        s.partial_merge   = 0.0f;
        s.partial_unidir  = 1.0f;
    }

    inline void update_camera_mis(State& s) {

    }

    inline void update_light_mis(State& s) {

    }

    inline void init_light_mis(const Ray& r, State& s) {

    }
};

} // namespace imba

#endif // IMBA_DEFERRED_VCM_H