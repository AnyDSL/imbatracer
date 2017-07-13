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
            // if (x > cam_.width() / 2) {
            //     x -= cam_.width() / 2;
            //     subwnd += 1;
            // }
            // if (y > cam_.height() / 2) {
            //     y -= cam_.height() / 2;
            //     subwnd += 2;
            // }

            // x *= 2;
            // y *= 2;

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

    rgb lo(0.0f);
    rgb hi(0.0f);
    float t = 0.0f;
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
            const auto isect = scene_.calculate_intersection(prim_rays.hit(i), prim_rays.ray(i));
            const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            const float vert_contrib = 3.0f;

            // Compute camera vertex density
            const int k = settings_.num_knn;
            auto photons = V_ARRAY(VertexHandle, k);
            int count = cam_grid_.query(isect.pos, photons, k);
            float radius_sqr = (count == k) ? lensqr(photons[k - 1].vert->isect.pos - isect.pos) : (radius_ * radius_);
            float cam_density = 0.0f;
            for (int i = 0; i < count; ++i) {
                // Epanechnikov filter
                const float d = lensqr(photons[i].vert->isect.pos - isect.pos);
                const float kernel = 1.0f - d / radius_sqr;
                cam_density += kernel * vert_contrib;
            }
            cam_density *= 2.0f / (pi * radius_sqr * num_cam_paths_);

            // Compute light vertex density
            count = light_grid_.query(isect.pos, photons, k);
            radius_sqr = (count == k) ? lensqr(photons[k - 1].vert->isect.pos - isect.pos) : (radius_ * radius_);
            float light_density = 0.0f;
            for (int i = 0; i < count; ++i) {
                // Epanechnikov filter
                const float d = lensqr(photons[i].vert->isect.pos - isect.pos);
                const float kernel = 1.0f - d / radius_sqr;
                light_density += kernel * vert_contrib;
            }
            light_density *= 2.0f / (pi * radius_sqr * num_light_paths_);

            // Add eye light shading
            rgb eye_light_clr(cos_theta_o, cos_theta_o, cos_theta_o);

            auto light_density_clr = rgb(light_density, 0, 0);//colorize(light_density);
            auto cam_density_clr   = rgb(0, 0, cam_density);//colorize(cam_density);
            auto prod_density_clr  = light_density_clr + cam_density_clr;

            if (!eye_light_)
                add_contribution(out, state.pixel_id, prod_density_clr);
            else
                add_contribution(out, state.pixel_id, eye_light_clr);

            // if (state.wnd == 0)
            //     add_contribution(out, state.pixel_id, eye_light_clr);
            // else if (state.wnd == 1)
            //     add_contribution(out, state.pixel_id, prod_density_clr);
            // else if (state.wnd == 2)
            //     add_contribution(out, state.pixel_id, cam_density_clr);
            // else
            //     add_contribution(out, state.pixel_id, light_density_clr);

            terminate_path(state);
        }
    });

    prim_rays.compact_rays();
}

} // namespace imba
