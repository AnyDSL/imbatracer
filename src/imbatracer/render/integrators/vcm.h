#ifndef IMBA_VCM_H
#define IMBA_VCM_H

#include "integrator.h"

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

inline float mis_heuristic(float a) {
    return a;
}

// Ray generator for light sources. Samples a point and a direction on a lightsource for every pixel sample.
class VCMLightRayGen : public PixelRayGen<VCMState> {
public:
    VCMLightRayGen(int w, int h, int n, LightContainer& lights) : PixelRayGen<VCMState>(w, h, n), lights_(lights) { }

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, VCMState& state_out) override {
        const float offset = 0.0001f;

        // randomly choose one light source to sample
        int i = state_out.rng.random_int(0, lights_.size());
        auto& l = lights_[i];
        float pdf_lightpick = 1.0f / lights_.size();

        Light::EmitSample sample = l->sample_emit(state_out.rng);
        ray_out.org.x = sample.pos.x;
        ray_out.org.y = sample.pos.y;
        ray_out.org.z = sample.pos.z;
        ray_out.org.w = offset;

        ray_out.dir.x = sample.dir.x;
        ray_out.dir.y = sample.dir.y;
        ray_out.dir.z = sample.dir.z;
        ray_out.dir.w = FLT_MAX;

        state_out.throughput = sample.radiance / pdf_lightpick;
        state_out.path_length = 1;
        state_out.continue_prob = 1.0f;

        state_out.dVCM = mis_heuristic(sample.pdf_direct_a / sample.pdf_emit_w); // pdf_lightpick cancels out

        if (l->is_delta())
            state_out.dVC = 0.0f;
        else
            state_out.dVC = mis_heuristic(sample.cos_out / (sample.pdf_emit_w * pdf_lightpick));

        state_out.dVM = state_out.dVC * mis_weight_vc_;

        state_out.is_finite = l->is_finite();
    }

    void set_mis_weight_vc(float w) { mis_weight_vc_ = w; }

private:
    LightContainer& lights_;
    float mis_weight_vc_;
};

/// Generates camera rays for VCM.
class VCMCameraRayGen : public PixelRayGen<VCMState> {
public:
    VCMCameraRayGen(PerspectiveCamera& cam, int spp)
        : PixelRayGen<VCMState>(cam.width(), cam.height(), spp), cam_(cam), light_path_count_(cam.width() * cam.height())
    {}

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, VCMState& state_out) override {
        // Sample a ray from the camera.
        const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
        const float sample_y = static_cast<float>(y) + state_out.rng.random_float();

        ray_out = cam_.generate_ray(sample_x, sample_y);

        state_out.throughput = float4(1.0f);
        state_out.path_length = 1;
        state_out.continue_prob = 1.0f;

        const float3 dir(ray_out.dir.x, ray_out.dir.y, ray_out.dir.z);

        // PDF on image plane is 1. We need to convert this from image plane area to solid angle.
        const float cos_theta_o = dot(dir, cam_.dir());
        assert(cos_theta_o > 0.0f);
        const float pdf_cam_w = sqr(cam_.image_plane_dist() / cos_theta_o) / cos_theta_o;

        state_out.dVC = 0.0f;
        state_out.dVM = 0.0f;
        state_out.dVCM = mis_heuristic(light_path_count_ / pdf_cam_w);
    }

private:
    PerspectiveCamera& cam_;
    const float light_path_count_;
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
    PhotonIterator(const LightPathContainer* cont = nullptr) : cont_(cont), cur_path_(0), cur_vertex_(0), is_end_(false) {}
    PhotonIterator(const LightPathContainer* cont, bool is_end) : cont_(cont), cur_path_(0), cur_vertex_(0), is_end_(is_end) {}
    PhotonIterator(const LightPathContainer* cont, int path, int vertex) : cont_(cont), cur_path_(path), cur_vertex_(vertex), is_end_(false) {}

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
    return cont_->light_paths_[cur_path_][cur_vertex_];
}

inline const LightPathVertex* PhotonIterator::operator-> () const {
    return &(cont_->light_paths_[cur_path_][cur_vertex_]);
}

inline PhotonIterator& PhotonIterator::operator++ () {
    if (is_end_)
        return *this;

    auto maxlen = cont_->get_path_len(cur_path_);
    if (++cur_vertex_ >= maxlen) {
        if (++cur_path_ >= cont_->max_path_count_)
            is_end_ = true;
        cur_vertex_ = 0;
    }
    return *this;
}

// bidirectional path tracing
class VCMIntegrator : public Integrator {
    static constexpr int TARGET_RAY_COUNT = 64 * 1000;
    static constexpr int MAX_LIGHT_PATH_LEN = 5;
    static constexpr int MAX_CAMERA_PATH_LEN = 8;
public:
    VCMIntegrator(Scene& scene, PerspectiveCamera& cam, int spp, float base_radius=0.003f, float radius_alpha=0.75f)
        : Integrator(scene, cam, spp),
          width_(cam.width()),
          height_(cam.height()),
          n_samples_(spp),
          light_sampler_(width_, height_, n_samples_, scene.lights),
          camera_sampler_(cam, spp),
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
        camera_sampler_.set_target(TARGET_RAY_COUNT);
        light_sampler_.set_target(TARGET_RAY_COUNT);
        photon_grid_.reserve(width_ * height_);
    }

    virtual void render(Image& out) override;
    virtual void reset() override {
        pm_radius_ = base_radius_ * scene_.sphere.radius;
        cur_iteration_ = 0;
    }

private:
    int width_, height_;
    int n_samples_;
    float light_path_count_;

    float base_radius_;
    float radius_alpha_;

    // Data for the current iteration
    int cur_iteration_;
    float pm_radius_;
    float vm_normalization_;
    float mis_weight_vc_;
    float mis_weight_vm_;

    VCMLightRayGen light_sampler_;
    VCMCameraRayGen camera_sampler_;

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

} // namespace imba

#endif

