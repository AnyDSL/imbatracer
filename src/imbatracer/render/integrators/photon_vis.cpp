#include "photon_vis.h"

namespace imba {

void PhotonVis::render(AtomicImage& img) {
    scheduler_.run_iteration(img,
        [this] (RayQueue<ShadowState>& ray_in, AtomicImage& out) { process_shadow_rays(ray_in, out); },
        [this] (RayQueue<PhotonVisState>& ray_in, RayQueue<ShadowState>& ray_out_shadow, AtomicImage& out) {
            process_camera_rays(ray_in, ray_out_shadow, out);
        },
        [this] (int x, int y, ::Ray& ray_out, PhotonVisState& state_out) {
            // Sample a ray from the camera.
            const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
            const float sample_y = static_cast<float>(y) + state_out.rng.random_float();
            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = rgb(1.0f);
        });
}

void PhotonVis::process_camera_rays(RayQueue<PhotonVisState>& prim_rays, RayQueue<ShadowState>& shadow_rays, AtomicImage& out) {
    auto states = prim_rays.states();
    const Hit* hits = prim_rays.hits();
    Ray* rays = prim_rays.rays();

    // Shrink the queue to only contain valid hits.
    const int hit_count = prim_rays.compact_hits();
    prim_rays.shrink(hit_count);

    tbb::parallel_for(tbb::blocked_range<int>(0, prim_rays.size()), [&] (const tbb::blocked_range<int>& range) {
        for (auto i = range.begin(); i != range.end(); ++i) {
            auto& state = prim_rays.state(i);
            RNG& rng = state.rng;
            const auto isect = calculate_intersection(scene_, prim_rays.hit(i), prim_rays.ray(i));
            const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            // Add eye light shading
            rgb eye_light(cos_theta_o, cos_theta_o, cos_theta_o);

            // Compute the contribution of nearby photons via density estimation.
            const int k = settings_.num_knn;
            auto photons = V_ARRAY(const PathLoader::DebugPhoton*, k);
            int count = accel_.query(isect.pos, photons, k);
            const float radius_sqr = (count == k) ? lensqr(photons[k - 1]->pos - isect.pos) : (radius_ * radius_);

            rgb contrib_pm(0.0f);
            rgb contrib_vc(0.0f);
            for (int i = 0; i < count; ++i) {
                auto p = photons[i];

                // Epanechnikov filter
                const float d = lensqr(p->pos - isect.pos);
                const float kernel = 1.0f - d / radius_sqr;

                contrib_pm += kernel * p->contrib_pm;
                contrib_vc += kernel * p->contrib_vc;
            }

            if (count == 0) {
                //add_contribution(out, states[i].pixel_id, eye_light * 0.2f);
                terminate_path(state);
                continue;
            }

            // Complete the Epanechnikov kernel
            contrib_pm *= 2.0f / (pi * radius_sqr * pl_.num_paths());
            contrib_vc *= 2.0f / (pi * radius_sqr * pl_.num_paths());

            // Colorize the contributions
            float pm_lum = (luminance(contrib_pm)) / (pl_.max_luminance_pm());
            float vc_lum = (luminance(contrib_vc)) / (pl_.max_luminance_vc());

            if (is_black(contrib_pm)) pm_lum = 0.0f;
            if (is_black(contrib_vc)) vc_lum = 0.0f;

            pm_lum = std::max(0.0f, pm_lum);
            pm_lum *= count;
            vc_lum = std::max(0.0f, vc_lum);
            vc_lum *= count;

            auto pm_col = rgb(0,0.5,0) * (pm_lum) + (1.0f - (pm_lum)) * rgb(0.5,0,0);
            auto vc_col = rgb(0,0.5,0) * (vc_lum) + (1.0f - (vc_lum)) * rgb(0.5,0,0);

            add_contribution(out, states[i].pixel_id, pm_col * 0.98f + eye_light * 0.02f);
            terminate_path(state);
        }
    });

    prim_rays.compact_rays();
}

} // namespace imba
