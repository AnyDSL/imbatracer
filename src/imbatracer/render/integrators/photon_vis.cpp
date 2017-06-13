#include "photon_vis.h"

namespace imba {

void PhotonVis::render(AtomicImage& img) {
    scheduler_.run_iteration(img,
        [this] (RayQueue<ShadowState>& ray_in, AtomicImage& out) { process_shadow_rays(ray_in, out); },
        [this] (RayQueue<PhotonVisState>& ray_in, RayQueue<ShadowState>& ray_out_shadow, AtomicImage& out) {
            process_camera_rays(ray_in, ray_out_shadow, out);
        },
        [this] (int x, int y, ::Ray& ray_out, PhotonVisState& state_out) -> bool {
            int subwnd = 0;
            if (x > cam_.width() / 2) {
                x -= cam_.width() / 2;
                subwnd += 1;
            }
            if (y > cam_.height() / 2) {
                y -= cam_.height() / 2;
                subwnd += 2;
            }

            x *= 2;
            y *= 2;

            // Sample a ray from the camera.
            const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
            const float sample_y = static_cast<float>(y) + state_out.rng.random_float();
            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = rgb(1.0f);
            state_out.wnd = subwnd;

            return true;
        });
}

struct GradientStep {
    float value;
    rgb color;
};

inline rgb colorize(float v) {
    static GradientStep steps[] {
        { 0.0f , { 0.0f, 0.0f, 0.0f} },
        { 0.25f, { 0.0f, 0.0f, 1.0f} },
        { 0.5f , { 0.0f, 1.0f, 0.0f} },
        { 0.75f, { 1.0f, 1.0f, 0.0f} },
        { 1.0f , { 1.0f, 0.0f, 0.0f} },
    };

    static int count = sizeof(steps) / sizeof(GradientStep);

    v = std::min(0.99f, std::max(0.0f, v));

    rgb lo;
    rgb hi;
    float t;
    for (int i = 0; i < count; ++i) {
        if (v < steps[i].value) {
            hi = steps[i].color;
            lo = steps[i - 1].color;

            t = v - steps[i - 1].value;
            t /= steps[i].value - steps[i - 1].value;

            break;
        }
    }

    return lerp(lo, hi, t);
}

void PhotonVis::process_camera_rays(RayQueue<PhotonVisState>& prim_rays, RayQueue<ShadowState>& shadow_rays, AtomicImage& out) {
    // Shrink the queue to only contain valid hits.
    const int hit_count = prim_rays.compact_hits();
    prim_rays.shrink(hit_count);

    tbb::parallel_for(tbb::blocked_range<int>(0, prim_rays.size()), [&] (const tbb::blocked_range<int>& range) {
        for (auto i = range.begin(); i != range.end(); ++i) {
            auto& state = prim_rays.state(i);
            const auto isect = calculate_intersection(scene_, prim_rays.hit(i), prim_rays.ray(i));
            const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            // Add eye light shading
            rgb eye_light(cos_theta_o, cos_theta_o, cos_theta_o);

            auto connect_contrib = colorize(grid_light_(isect.pos, 1));
            auto light_contrib   = colorize(grid_light_(isect.pos, 0));
            auto light_density   = colorize(grid_light_(isect.pos, 2));
            auto joint_contrib   = colorize(grid_light_(isect.pos, 0) + grid_light_(isect.pos, 1));
            //auto prod_power      = colorize(grid_light_(isect.pos, 3) * grid_cam_(isect.pos, 0));
            if (state.wnd == 0)
                add_contribution(out, state.pixel_id, joint_contrib);
            else if (state.wnd == 1)
                add_contribution(out, state.pixel_id, connect_contrib);
            else if (state.wnd == 2)
                add_contribution(out, state.pixel_id, light_contrib);
            else
                add_contribution(out, state.pixel_id, light_density);

            terminate_path(state);
        }
    });

    prim_rays.compact_rays();
}

} // namespace imba
