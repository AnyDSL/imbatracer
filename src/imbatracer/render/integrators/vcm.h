#ifndef IMBA_VCM_H
#define IMBA_VCM_H

#include "integrator.h"
#include "../ray_scheduler.h"
#include "../ray_gen.h"
#include "../../rangesearch/rangesearch.h"

//#define QUEUE_SCHEDULING

namespace imba {

struct VCMState : RayState {
    float4 throughput;
    int path_length : 7;
    bool is_finite : 1; // Used to store whether the light source during light tracing was finite.

    // Russian roulette probability for continuing this path.
    float continue_prob;

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

enum VCMSubAlgorithm {
    ALGO_VCM,
    ALGO_BPT,
    ALGO_PPM,
    ALGO_LT,
    ALGO_PT
};

template<VCMSubAlgorithm algo>
class VCMIntegrator : public Integrator {
    // Number of light paths to be traced when computing the average length and thus vertex cache size.
    static constexpr int LIGHT_PATH_LEN_PROBES = 10000;
    static constexpr int MAX_NUM_CONNECTIONS = 8;
public:
    VCMIntegrator(Scene& scene, PerspectiveCamera& cam, RayGen<VCMState>& ray_gen,
        int max_path_len, int thread_count, int tile_size, int spp, int num_connections, float base_radius=0.03f, float radius_alpha=0.75f)
        : Integrator(scene, cam)
        , width_(cam.width())
        , height_(cam.height())
        , ray_gen_(ray_gen)
        , light_path_count_(cam.width() * cam.height())
        , pm_radius_(base_radius)
        , base_radius_(base_radius)
        , radius_alpha_(radius_alpha)
        , cur_iteration_(0)
#ifdef QUEUE_SCHEDULING
        , scheduler_(ray_gen, scene)
#else
        , scheduler_(ray_gen, scene, thread_count, tile_size)
#endif
        , max_path_len_(max_path_len)
        , spp_(spp)
        , vertex_caches_(spp)
        , vertex_cache_last_(spp)
        , light_vertices_count_(spp)
        , photon_grid_(spp)
        , num_connections_(num_connections)
    {
        for (auto& grid : photon_grid_)
            grid.reserve(cam.width() * cam.height());
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
    const int num_connections_;

    float base_radius_;
    float radius_alpha_;

    // Data for the current iteration
    int cur_iteration_;
    float pm_radius_;
    float vm_normalization_;
    float mis_weight_vc_;
    float mis_weight_vm_;

    RayGen<VCMState>& ray_gen_;

#ifdef QUEUE_SCHEDULING
    QueueScheduler<VCMState, 8, MAX_NUM_CONNECTIONS + 1> scheduler_;
#else
    TileScheduler<VCMState, MAX_NUM_CONNECTIONS + 1> scheduler_;
#endif

    // Light path vertices and associated data are stored separately per sample / iteration.
    std::vector<std::vector<LightPathVertex> > vertex_caches_;
    std::vector<std::atomic<int> > vertex_cache_last_;
    std::vector<int> light_vertices_count_;
    std::vector<HashGrid<PhotonIterator> > photon_grid_;

    void reset_buffers();
    void add_vertex_to_cache(const LightPathVertex& v, int sample_id);

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

using VCM    = VCMIntegrator<ALGO_VCM>;
using BPT    = VCMIntegrator<ALGO_BPT>;
using PPM    = VCMIntegrator<ALGO_PPM>;
using LT     = VCMIntegrator<ALGO_LT >;
using VCM_PT = VCMIntegrator<ALGO_PT >;

} // namespace imba

#endif

