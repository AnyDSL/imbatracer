#ifndef IMBA_BPT_H
#define IMBA_BPT_H

#include "integrator.h"

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

inline float mis_heuristic_bpt(float a) {
    return powf(a, 2.0f);
}

// Ray generator for light sources. Samples a point and a direction on a lightsource for every pixel sample.
class BPTLightRayGen : public PixelRayGen<BPTState> {
public:
    BPTLightRayGen(int w, int h, int n, LightContainer& lights) : PixelRayGen<BPTState>(w, h, n), lights_(lights) { }

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, BPTState& state_out) override {
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

        state_out.dVCM = mis_heuristic_bpt(sample.pdf_direct_a / sample.pdf_emit_w); // pdf_lightpick cancels out

        if (l->is_delta())
            state_out.dVC = 0.0f;
        else
            state_out.dVC = mis_heuristic_bpt(sample.cos_out / (sample.pdf_emit_w * pdf_lightpick));

        state_out.is_finite = l->is_finite();
    }

private:
    LightContainer& lights_;
};

/// Generates camera rays for BPT.
class BPTCameraRayGen : public PixelRayGen<BPTState> {
public:
    BPTCameraRayGen(PerspectiveCamera& cam, int spp)
        : PixelRayGen<BPTState>(cam.width(), cam.height(), spp), cam_(cam), light_path_count_(cam.width() * cam.height())
    {}

    virtual void sample_pixel(int x, int y, ::Ray& ray_out, BPTState& state_out) override {
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
        state_out.dVCM = mis_heuristic_bpt(light_path_count_ / pdf_cam_w);
    }

private:
    PerspectiveCamera& cam_;
    const float light_path_count_;
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
    static constexpr int TARGET_RAY_COUNT = 64 * 1000;
    static constexpr int MAX_LIGHT_PATH_LEN = 5;
    static constexpr int MAX_CAMERA_PATH_LEN = 8;
public:
    BidirPathTracer(Scene& scene, PerspectiveCamera& cam, int spp, float base_radius=0.1f, float radius_alpha=0.75f)
        : Integrator(scene, cam, spp),
          width_(cam.width()),
          height_(cam.height()),
          n_samples_(spp),
          light_sampler_(width_, height_, n_samples_, scene.lights),
          camera_sampler_(cam, spp),
          primary_rays_{ RayQueue<BPTState>(TARGET_RAY_COUNT), RayQueue<BPTState>(TARGET_RAY_COUNT)},
          shadow_rays_(TARGET_RAY_COUNT * (MAX_LIGHT_PATH_LEN + 1)),
          light_image_(width_, height_),
          light_path_count_(width_ * height_),
          light_paths_(MAX_LIGHT_PATH_LEN, width_ * height_),
          pm_radius_(base_radius),
          base_radius_(base_radius * scene_.sphere.radius),
          radius_alpha_(radius_alpha),
          cur_iteration_(0)
    {
        camera_sampler_.set_target(TARGET_RAY_COUNT);
        light_sampler_.set_target(TARGET_RAY_COUNT);
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

    BPTLightRayGen light_sampler_;
    BPTCameraRayGen camera_sampler_;

    BPTLightPathContainer light_paths_;

    void reset_buffers();

    RayQueue<BPTState> primary_rays_[2];
    RayQueue<BPTState> shadow_rays_;
    RayQueue<BPTState> light_rays_[2];

    Image light_image_;

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

