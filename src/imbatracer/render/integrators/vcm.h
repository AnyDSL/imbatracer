#ifndef IMBA_VCM_H
#define IMBA_VCM_H

#include "integrator.h"
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

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
    float dVM;

    LightPathVertex(Intersection isect, float4 tp, float continue_prob, float dVC, float dVCM, float dVM)
        : isect(isect), throughput(tp), continue_prob(continue_prob), dVC(dVC), dVCM(dVCM), dVM(dVM)
    {}

    LightPathVertex() {}

    float3& position() { return isect.pos; }
    const float3& position() const { return isect.pos; }
};

using LightPath = std::vector<LightPathVertex>;

class LightPathContainer;

/// Iterator class for iterating over all the photons in a LightPathContainer
class PhotonIterator {
public:
    PhotonIterator() : cont_(nullptr), cur_path_(-1), cur_vertex_(-1), is_end_(true) {}
    PhotonIterator(const LightPathContainer* cont, bool is_end = false);

    const LightPathVertex& operator* () const;
    const LightPathVertex* operator-> () const;
    PhotonIterator& operator++ ();

    bool operator!= (const PhotonIterator& r) const {
        return !(*this == r);
    }

    bool operator== (const PhotonIterator& r) const {
        return ((!r.is_end_ && !is_end_) && (r.cur_path_ == cur_path_ && r.cur_vertex_ == cur_vertex_)) || (r.is_end_ && is_end_);
    }

private:
    const LightPathContainer* cont_;
    int cur_path_;
    int cur_vertex_;
    bool is_end_;
};

class LightPathContainer {
public:
    LightPathContainer(int max_len, int max_path_count)
        : max_len_(max_len), max_path_count_(max_path_count),
          light_paths_(max_path_count), light_path_lengths_(max_path_count)
    {
        for (auto& v : light_paths_) {
            v.resize(max_len);
        }
    }

    ~LightPathContainer() {}

    LightPathContainer(const LightPathContainer&) = delete;
    LightPathContainer& operator= (const LightPathContainer&) = delete;
    LightPathContainer(LightPathContainer&&) = delete;
    LightPathContainer& operator= (LightPathContainer&&) = delete;

    void reset() {
        std::fill(light_path_lengths_.begin(), light_path_lengths_.end(), 0);
    }

    int get_path_len(int pixel_index) const {
        return light_path_lengths_[pixel_index];
    }

    const LightPath& get_path(int pixel_index) const {
        return light_paths_[pixel_index];
    }

    template<typename... Args>
    void append(int pixel_index, Args&&... args) {
        light_paths_[pixel_index][get_path_len(pixel_index)] = LightPathVertex(std::forward<Args>(args)...);
        ++light_path_lengths_[pixel_index];

        assert(get_path_len(pixel_index) <= max_len_);
    }

    PhotonIterator begin() {
        return PhotonIterator(this);
    }

    PhotonIterator end() {
        return PhotonIterator(this, true);
    }

    int size() {
        int counter = 0;
        for (PhotonIterator it = begin(); it != end(); ++it)
            counter++;
        return counter;
    }

private:
    int max_len_;
    int max_path_count_;

    std::vector<LightPath> light_paths_;
    std::vector<int> light_path_lengths_;

    friend class PhotonIterator;
};

inline const LightPathVertex& PhotonIterator::operator* () const {
    assert(!is_end_);
    assert(cur_path_ < cont_->light_paths_.size() && cur_vertex_ < cont_->get_path_len(cur_path_));
    return cont_->light_paths_[cur_path_][cur_vertex_];
}

inline const LightPathVertex* PhotonIterator::operator-> () const {
    assert(!is_end_);
    assert(cur_path_ < cont_->light_paths_.size());
    assert(cur_vertex_ < cont_->get_path_len(cur_path_));
    return &(cont_->light_paths_[cur_path_][cur_vertex_]);
}

inline PhotonIterator& PhotonIterator::operator++ () {
    if (is_end_)
        return *this;

    auto maxlen = cont_->get_path_len(cur_path_);
    if (++cur_vertex_ >= maxlen) {
        // Skip empty paths
        while (++cur_path_ < cont_->max_path_count_ && cont_->get_path_len(cur_path_) <= 0) ;

        if (cur_path_ >= cont_->max_path_count_)
            is_end_ = true;

        cur_vertex_ = 0;
    }
    return *this;
}

inline PhotonIterator::PhotonIterator(const LightPathContainer* cont, bool is_end) : cont_(cont), cur_path_(0), cur_vertex_(0), is_end_(is_end) {
    if (!is_end) {
        while (cur_path_ < cont_->max_path_count_ && cont_->get_path_len(cur_path_) <= 0)
            ++cur_path_;

        if (cur_path_ >= cont_->max_path_count_)
            is_end_ = true;
    }
}

// bidirectional path tracing
template<bool bpt_only>
class VCMIntegrator : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 1 << 20;
    static constexpr int MAX_LIGHT_PATH_LEN = 5;
    static constexpr int MAX_CAMERA_PATH_LEN = 8;
public:
    VCMIntegrator(Scene& scene, PerspectiveCamera& cam, RayGen<VCMState>& ray_gen, float base_radius=0.003f, float radius_alpha=0.75f)
        : Integrator(scene, cam),
          width_(cam.width()),
          height_(cam.height()),
          ray_gen_(ray_gen),
          primary_rays_{ RayQueue<VCMState>(TARGET_RAY_COUNT), RayQueue<VCMState>(TARGET_RAY_COUNT)},
          shadow_rays_(TARGET_RAY_COUNT * (MAX_LIGHT_PATH_LEN + 1)),
          light_image_(width_, height_),
          pm_image_(width_, height_),
          light_path_count_(width_ * height_),
          light_paths_(MAX_LIGHT_PATH_LEN, width_ * height_),
          pm_radius_(base_radius),
          base_radius_(base_radius),
          radius_alpha_(radius_alpha),
          cur_iteration_(0)
    {
        photon_grid_.reserve(width_ * height_);
    }

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
    float mis_weight_vc_;
    float mis_weight_vm_;

    RayGen<VCMState>& ray_gen_;
    LightPathContainer light_paths_;

    void reset_buffers();

    RayQueue<VCMState> primary_rays_[2];
    RayQueue<VCMState> shadow_rays_;
    RayQueue<VCMState> light_rays_[2];

    Image light_image_;
    Image pm_image_;

    HashGrid<PhotonIterator> photon_grid_;

    void process_light_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& rays_out, RayQueue<VCMState>& rays_out_shadow);
    void process_camera_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& rays_out, RayQueue<VCMState>& shadow_rays, Image& img);
    void process_shadow_rays(RayQueue<VCMState>& rays_in, Image& img);

    void trace_light_paths();
    void trace_camera_paths(Image& img);

    void connect_to_camera(const VCMState& light_state, const Intersection& isect, const BSDF* bsdf, RayQueue<VCMState>& rays_out_shadow);

    void direct_illum(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out_shadow);
    void connect(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, MemoryArena& bsdf_arena, RayQueue<VCMState>& rays_out_shadow);
    void vertex_merging(const VCMState& state, const Intersection& isect, const BSDF* bsdf, Image& img);

    void bounce(VCMState& state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out, bool adjoint, int max_length);
};

using VCM = VCMIntegrator<false>;
using BPT = VCMIntegrator<true>;

} // namespace imba

#endif

