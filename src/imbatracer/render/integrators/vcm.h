#ifndef IMBA_VCM_H
#define IMBA_VCM_H

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/scheduling/tile_scheduler.h"
#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/rangesearch/rangesearch.h"
#include "imbatracer/render/integrators/light_vertices.h"

#include "imbatracer/render/debug/path_debug.h"
#include "imbatracer/render/debug/mis_debug.h"

#include "imbatracer/frontend/cmd_line.h"

// Enable this to write light path information to a file after each frame (SLOW!)
#define LIGHT_PATH_DEBUG false

// Enable this to write the individual contributions from the techniques to separate images
#define TECHNIQUES_DEBUG false

namespace imba {

/// Stores the current state of a ray during VCM or any sub-algorithm of VCM.
struct VCMState : RayState {
    rgb throughput;
    int path_length : 31;
    bool finite_light : 1;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
    float dVM;
};

enum VCMSubAlgorithm {
    ALGO_VCM,
    ALGO_BPT,
    ALGO_PPM,
    ALGO_LT,
    ALGO_PT
};

struct VCMShadowState : ShadowState {
#if TECHNIQUES_DEBUG
    int sample_id;
    float weight;
    int technique;
#endif
};

template <VCMSubAlgorithm algo>
class VCMIntegrator : public Integrator {
public:
    VCMIntegrator(Scene& scene, PerspectiveCamera& cam, RayScheduler<VCMState, VCMShadowState>& scheduler, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , cur_iteration_(0)
        , scheduler_(scheduler)
        , light_vertices_(settings.light_path_count)
        , light_tile_gen_(scene.light_count(), settings.light_path_count, settings.tile_size * settings.tile_size)
        , light_scheduler_(light_tile_gen_, scene, 1, settings.thread_count, settings.tile_size * settings.tile_size * 1.75f,
                           settings.traversal_platform == UserSettings::gpu) // TODO: make threshold explicit in TileGen
    {
    }

    virtual void render(AtomicImage& out) override;
    virtual void reset() override {
        pm_radius_ = settings_.base_radius * scene_.bounding_sphere().radius;
        cur_iteration_ = 0;
    }

    virtual void preprocess() override {
        if (algo != ALGO_LT && algo != ALGO_PT)
            light_vertices_.compute_cache_size(scene_, scheduler_.gpu_traversal);
    }

private:
    const UserSettings settings_;

    // Data for the current iteration
    int cur_iteration_;
    float pm_radius_;
    float vm_normalization_;
    float mis_eta_vc_;
    float mis_eta_vm_;

    // Debugging tools
    enum SamplingTechniques {
        merging,
        connecting,
        next_event,
        cam_connect,
        light_hit,
        technique_count
    };

    PathDebugger<VCMState, LIGHT_PATH_DEBUG> light_path_dbg_;
    MISDebugger<technique_count, TECHNIQUES_DEBUG> techniques_dbg_;

    // Scheduling
    UniformLightTileGen<VCMState> light_tile_gen_;
    RayScheduler<VCMState, VCMShadowState>& scheduler_;
    TileScheduler<VCMState, VCMShadowState> light_scheduler_;
    LightVertices light_vertices_;

    /// Computes the power for the power heuristic.
    inline float mis_pow(float a) {
        constexpr float power = 1.0f;
        return powf(a, power);
    }

    /// Computes the cosine term for adjoint BSDFs that use shading normals.
    ///
    /// This function has to be used for all BSDFs while tracing paths from the light sources, to prevent brighness discontinuities.
    /// See Veach's thesis for more details.
    inline static float shading_normal_adjoint(const float3& normal, const float3& geom_normal, const float3& out_dir, const float3& in_dir) {
        return dot(out_dir, normal) * dot(in_dir, geom_normal) / dot(out_dir, geom_normal);
    }

    void process_light_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMShadowState>& rays_out_shadow, AtomicImage& img);
    void process_camera_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMShadowState>& shadow_rays, AtomicImage& img);

    void trace_light_paths(AtomicImage& img);
    void trace_camera_paths(AtomicImage& img);

    void connect_to_camera(const VCMState& light_state, const Intersection& isect, const BSDF* bsdf, RayQueue<VCMShadowState>& rays_out_shadow);

    void direct_illum(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMShadowState>& rays_out_shadow);
    void connect(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, MemoryArena& bsdf_arena, RayQueue<VCMShadowState>& rays_out_shadow);
    void vertex_merging(const VCMState& state, const Intersection& isect, const BSDF* bsdf, AtomicImage& img);

    void bounce(VCMState& state, const Intersection& isect, BSDF* bsdf, Ray& rays_out, bool adjoint, float offset);

    void process_shadow_rays_dbg(RayQueue<VCMShadowState>& ray_in, AtomicImage& out);
};

using VCM    = VCMIntegrator<ALGO_VCM>;
using BPT    = VCMIntegrator<ALGO_BPT>;
using PPM    = VCMIntegrator<ALGO_PPM>;
using LT     = VCMIntegrator<ALGO_LT >;
using VCM_PT = VCMIntegrator<ALGO_PT >;

} // namespace imba

#endif // IMBA_VCM_H
