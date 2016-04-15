#ifndef IMBA_VCM_H
#define IMBA_VCM_H

#include "integrator.h"
#include "../ray_scheduler.h"
#include "../ray_gen.h"
#include "../../rangesearch/rangesearch.h"

namespace imba {

struct VCMState : RayState {
    float4 throughput;
    int path_length;

    // Russian roulette probability for continuing this path.
    float continue_prob;

    // Used to store whether the light source during light tracing was finite.
    bool is_finite;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
    float dVM;
};

struct LightPathVertex {
    Intersection isect;
    float4 throughput;

    float continue_prob;
    int path_length;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
    float dVM;

    LightPathVertex(Intersection isect, float4 tp, float continue_prob, float dVC, float dVCM, float dVM, int path_length)
        : isect(isect), throughput(tp), continue_prob(continue_prob), dVC(dVC), dVCM(dVCM), dVM(dVM), path_length(path_length)
    {}

    LightPathVertex() {}

    float3& position() { return isect.pos; }
    const float3& position() const { return isect.pos; }
};

using PhotonIterator = std::vector<LightPathVertex>::iterator;

template<bool bpt_only, bool ppm_only, bool lt_only, bool pt_only>
class VCMIntegrator : public Integrator {
    // Number of light paths to be traced when computing the average length and thus vertex cache size.
    static constexpr int LIGHT_PATH_LEN_PROBES = 10000;
    static constexpr int NUM_CONNECTIONS = 4;
public:
    VCMIntegrator(Scene& scene, PerspectiveCamera& cam, RayGen<VCMState>& ray_gen,
        int max_path_len, int thread_count, int tile_size, int spp, float base_radius=0.03f, float radius_alpha=0.75f)
        : Integrator(scene, cam)
        , width_(cam.width())
        , height_(cam.height())
        , ray_gen_(ray_gen)
        , light_path_count_(cam.width() * cam.height() * spp)
        , pm_radius_(base_radius)
        , base_radius_(base_radius)
        , radius_alpha_(radius_alpha)
        , cur_iteration_(0)
        , scheduler_(ray_gen, scene, thread_count, tile_size)
        , max_path_len_(max_path_len)
        , spp_(spp)
        , photon_grid_()
    {
        photon_grid_.reserve(cam.width() * cam.height() * spp);
    }

    virtual void render(AtomicImage& out) override;
    virtual void reset() override {
        pm_radius_ = base_radius_ * scene_.sphere.radius;
        cur_iteration_ = 0;
    }

    virtual void preprocess() override;

private:
    int width_, height_;
    int spp_;
    float light_path_count_;
    const int max_path_len_;

    float base_radius_;
    float radius_alpha_;

    // Data for the current iteration
    int cur_iteration_;
    float pm_radius_;
    float vm_normalization_;
    float mis_weight_vc_;
    float mis_weight_vm_;

    RayGen<VCMState>& ray_gen_;
    TileScheduler<VCMState, NUM_CONNECTIONS + 1> scheduler_;

    std::vector<LightPathVertex> vertex_cache_;
    std::atomic<int> vertex_cache_last_;
    int light_vertices_count_;
    HashGrid<PhotonIterator> photon_grid_;

    void reset_buffers();
    void add_vertex_to_cache(const LightPathVertex& v);

    void process_light_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& rays_out, RayQueue<VCMState>& rays_out_shadow, AtomicImage& img);
    void process_camera_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& rays_out, RayQueue<VCMState>& shadow_rays, AtomicImage& img);
    void process_shadow_rays(RayQueue<VCMState>& rays_in, AtomicImage& img);

    void trace_light_paths(AtomicImage& img);
    void trace_camera_paths(AtomicImage& img);

    void connect_to_camera(const VCMState& light_state, const Intersection& isect, const BSDF* bsdf, RayQueue<VCMState>& rays_out_shadow);

    void direct_illum(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out_shadow);
    void connect(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, MemoryArena& bsdf_arena, RayQueue<VCMState>& rays_out_shadow);
    void vertex_merging(const VCMState& state, const Intersection& isect, const BSDF* bsdf, AtomicImage& img);

    void bounce(VCMState& state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out, bool adjoint);

    template<typename StateType, int queue_count, int shadow_queue_count, int max_shadow_rays_per_hit>
    friend class RayScheduler;
};

using VCM    = VCMIntegrator<false, false, false, false>;
using BPT    = VCMIntegrator<true , false, false, false>;
using PPM    = VCMIntegrator<false, true , false, false>;
using LT     = VCMIntegrator<false, false, true , false>;
using VCM_PT = VCMIntegrator<false, false, false, true >;

} // namespace imba

#endif

