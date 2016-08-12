#ifndef IMBA_VCM_H
#define IMBA_VCM_H

#include "integrator.h"
#include "../ray_scheduler.h"
#include "../ray_gen.h"

#include "../../rangesearch/rangesearch.h"
#include "light_vertices.h"

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

template <VCMSubAlgorithm algo>
class VCMIntegrator : public Integrator {
public:
    VCMIntegrator(Scene& scene, PerspectiveCamera& cam, RayScheduler<VCMState>& scheduler,
        int max_path_len, int spp, int num_connections, float base_radius=0.03f, float radius_alpha=0.75f)
        : Integrator(scene, cam)
        , width_(cam.width())
        , height_(cam.height())
        , light_path_count_(cam.width() * cam.height())
        , pm_radius_(base_radius)
        , base_radius_(base_radius)
        , radius_alpha_(radius_alpha)
        , cur_iteration_(0)
        , scheduler_(scheduler)
        , max_path_len_(max_path_len)
        , spp_(spp)
        , num_connections_(num_connections)
        , light_vertices_(cam.width() * cam.height(), spp)
    {
    }

    virtual void render(AtomicImage& out) override;
    virtual void reset() override {
        pm_radius_ = base_radius_ * scene_.bounding_sphere().radius;
        cur_iteration_ = 0;
    }

    virtual void preprocess() override {
        if (algo != ALGO_LT && algo != ALGO_PT)
            light_vertices_.compute_cache_size(scene_);
    }

private:
    // Maximum path length and number of connections. Can be used to tweak performance.
    const int max_path_len_;
    const int num_connections_;

    // Information on the current image.
    int width_, height_;
    int spp_;

    float light_path_count_;
    float base_radius_;
    float radius_alpha_;

    // Data for the current iteration
    int cur_iteration_;
    float pm_radius_;
    float vm_normalization_;
    float mis_eta_vc_;
    float mis_eta_vm_;

    RayScheduler<VCMState>& scheduler_;

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

    void process_light_rays(RayQueue<VCMState>& rays_in, RayQueue<ShadowState>& rays_out_shadow, AtomicImage& img);
    void process_camera_rays(RayQueue<VCMState>& rays_in, RayQueue<ShadowState>& shadow_rays, AtomicImage& img);
    void process_shadow_rays(RayQueue<ShadowState>& rays_in, AtomicImage& img);

    void trace_light_paths(AtomicImage& img);
    void trace_camera_paths(AtomicImage& img);

    void connect_to_camera(const VCMState& light_state, const Intersection& isect, const BSDF* bsdf, RayQueue<ShadowState>& rays_out_shadow);

    void direct_illum(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<ShadowState>& rays_out_shadow);
    void connect(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, MemoryArena& bsdf_arena, RayQueue<ShadowState>& rays_out_shadow);
    void vertex_merging(const VCMState& state, const Intersection& isect, const BSDF* bsdf, AtomicImage& img);

    void bounce(VCMState& state, const Intersection& isect, BSDF* bsdf, Ray& rays_out, bool adjoint);
};

using VCM    = VCMIntegrator<ALGO_VCM>;
using BPT    = VCMIntegrator<ALGO_BPT>;
using PPM    = VCMIntegrator<ALGO_PPM>;
using LT     = VCMIntegrator<ALGO_LT >;
using VCM_PT = VCMIntegrator<ALGO_PT >;

} // namespace imba

#endif // IMBA_VCM_H
