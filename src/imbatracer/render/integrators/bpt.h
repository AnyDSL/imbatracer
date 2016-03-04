#ifndef IMBA_BPT_H
#define IMBA_BPT_H

#include "integrator.h"
#include "../ray_gen.h"

namespace imba {

struct BPTState : RayState {
    float4 throughput;
    int path_length;

    // Russian roulette probability for continuing this path.
    float continue_prob;

    // Used to store whether the light source during light tracing was finite.
    bool is_finite;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
};

struct BPTLightPathVertex {
    Intersection isect;
    float4 throughput;

    float continue_prob;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;

    BPTLightPathVertex(Intersection isect, float4 tp, float continue_prob, float dVC, float dVCM)
        : isect(isect), throughput(tp), continue_prob(continue_prob), dVC(dVC), dVCM(dVCM)
    {}

    BPTLightPathVertex() {}
};

using BPTLightPath = std::vector<BPTLightPathVertex>;

class BPTLightPathContainer {
public:
    BPTLightPathContainer(int max_len, int max_path_count)
        : max_len_(max_len), max_path_count_(max_path_count),
          light_paths_(max_path_count), light_path_lengths_(max_path_count)
    {
        for (auto& v : light_paths_) {
            v.resize(max_len);
        }
    }

    ~BPTLightPathContainer() {}

    BPTLightPathContainer(const BPTLightPathContainer&) = delete;
    BPTLightPathContainer& operator= (const BPTLightPathContainer&) = delete;
    BPTLightPathContainer(BPTLightPathContainer&&) = delete;
    BPTLightPathContainer& operator= (BPTLightPathContainer&&) = delete;

    void reset() {
        std::fill(light_path_lengths_.begin(), light_path_lengths_.end(), 0);
    }

    int get_path_len(int pixel_index) const {
        return light_path_lengths_[pixel_index];
    }

    const BPTLightPath& get_path(int pixel_index) const {
        return light_paths_[pixel_index];
    }

    template<typename... Args>
    void append(int pixel_index, Args&&... args) {
        light_paths_[pixel_index][get_path_len(pixel_index)] = BPTLightPathVertex(std::forward<Args>(args)...);
        ++light_path_lengths_[pixel_index];

        assert(get_path_len(pixel_index) <= max_len_);
    }

private:
    int max_len_;
    int max_path_count_;

    std::vector<BPTLightPath> light_paths_;
    std::vector<int> light_path_lengths_;

    friend class PhotonIterator;
};

// bidirectional path tracing
class BidirPathTracer : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 1 << 20;
    static constexpr int MAX_LIGHT_PATH_LEN = 5;
    static constexpr int MAX_CAMERA_PATH_LEN = 8;
public:
    BidirPathTracer(Scene& scene, PerspectiveCamera& cam, RayGen<BPTState>& ray_gen, float base_radius=0.1f, float radius_alpha=0.75f)
        : Integrator(scene, cam),
          width_(cam.width()),
          height_(cam.height()),
          ray_gen_(ray_gen),
          primary_rays_{ RayQueue<BPTState>(TARGET_RAY_COUNT), RayQueue<BPTState>(TARGET_RAY_COUNT)},
          shadow_rays_(TARGET_RAY_COUNT * (MAX_LIGHT_PATH_LEN + 1)),
          light_image_(width_, height_),
          light_path_count_(width_ * height_),
          light_paths_(MAX_LIGHT_PATH_LEN, width_ * height_),
          pm_radius_(base_radius),
          base_radius_(base_radius * scene_.sphere.radius),
          radius_alpha_(radius_alpha),
          cur_iteration_(0)
    {}

    virtual void render(Image& out) override;

    virtual void reset() override {
        pm_radius_ = base_radius_ * scene_.sphere.radius;
        cur_iteration_ = 0;
    }

private:
    int width_, height_;
    float light_path_count_;

    float base_radius_;
    float radius_alpha_;

    // Data for the current iteration
    int cur_iteration_;
    float pm_radius_;
    float vm_normalization_;

    RayGen<BPTState>& ray_gen_;
    BPTLightPathContainer light_paths_;

    RayQueue<BPTState> primary_rays_[2];
    RayQueue<BPTState> shadow_rays_;
    RayQueue<BPTState> light_rays_[2];

    Image light_image_;

    void reset_buffers();

    void process_light_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& rays_out_shadow);
    void process_camera_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& shadow_rays, Image& img);
    void process_shadow_rays(RayQueue<BPTState>& rays_in, Image& img);

    void trace_light_paths();
    void trace_camera_paths(Image& img);

    void connect_to_camera(const BPTState& light_state, const Intersection& isect, BSDF* bsdf, RayQueue<BPTState>& rays_out_shadow);
    void direct_illum(BPTState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<BPTState>& rays_out_shadow);
    void connect(BPTState& cam_state, const Intersection& isect, BSDF* bsdf, MemoryArena& bsdf_arena, RayQueue<BPTState>& rays_out_shadow);
};

} // namespace imba

#endif

