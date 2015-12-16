#include "bpt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <cmath>

namespace imba {

void BidirPathTracer::render(Image& img) {
    reset_buffers();

    trace_light_paths();
    trace_camera_paths(img);

    // Merge light and camera images.
    for (int i = 0; i < width_ * height_; ++i)
        img.pixels()[i] += light_image_.pixels()[i];
}

void BidirPathTracer::reset_buffers() {
    for (auto& p : light_paths_) {
        for (auto& s : p) s.clear();
    }

    for (int i = 0; i < width_ * height_; ++i)
        light_image_.pixels()[i] = float4(0.0f);
}

void BidirPathTracer::trace_light_paths() {
    light_sampler_.start_frame();

    int in_queue = 0;
    int out_queue = 1;

    while(true) {
        light_sampler_.fill_queue(primary_rays_[in_queue]);

        if (primary_rays_[in_queue].size() <= 0)
            break;

        primary_rays_[in_queue].traverse(scene_);
        process_light_rays(primary_rays_[in_queue], primary_rays_[out_queue], shadow_rays_);
        primary_rays_[in_queue].clear();

        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays_.size() > 0) {
            shadow_rays_.traverse_occluded(scene_);
            process_shadow_rays(shadow_rays_, light_image_);
            shadow_rays_.clear();
        }

        std::swap(in_queue, out_queue);
    }
}

void BidirPathTracer::trace_camera_paths(Image& img) {
    // Create the initial set of camera rays.
    auto camera = camera_sampler_;
    camera.start_frame();

    int in_queue = 0;
    int out_queue = 1;

    while(true) {
        camera.fill_queue(primary_rays_[in_queue]);

        if (primary_rays_[in_queue].size() <= 0)
            break;

        primary_rays_[in_queue].traverse(scene_);
        process_camera_rays(primary_rays_[in_queue], primary_rays_[out_queue], shadow_rays_, img);
        primary_rays_[in_queue].clear();

        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays_.size() > 0) {
            shadow_rays_.traverse_occluded(scene_);
            process_shadow_rays(shadow_rays_, img);
            shadow_rays_.clear();
        }

        std::swap(in_queue, out_queue);
    }
}

void BidirPathTracer::process_light_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& ray_out_shadow) {
    constexpr float offset = 0.01f;

    int ray_count = rays_in.size();
    BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0)
            continue;

        RNG& rng = states[i].rng;

        Intersection isect = calculate_intersection(hits, rays, i);
        float cos_theta_i = dot(isect.out_dir, isect.surf.normal);

        // Complete calculation of the partial weights.
        if (states[i].path_length > 1)
            states[i].dVCM *= isect.distance * isect.distance;

        states[i].dVCM *= 1.0f / cos_theta_i;
        states[i].dVC  *= 1.0f / cos_theta_i;

        auto& light_path = light_paths_[states[i].pixel_id][states[i].sample_id];
        if (!isect.mat->is_delta) { // Do not store vertices on materials described by a delta distribution.
            light_path.emplace_back(
                isect,
                states[i].throughput,
                states[i].dVC,
                states[i].dVCM
            );

            connect_to_camera(states[i], isect, ray_out_shadow);
        }

        // Decide wether or not to continue this path using russian roulette and a fixed maximum length.
        const float4 srgb(0.2126f, 0.7152f, 0.0722f, 0.0f);
        const float kill_prob = dot(states[i].throughput, srgb) * 100.0f;
        const float rrprob = std::min(1.0f, kill_prob);
        const float u_rr = rng.random_float();
        if (u_rr > rrprob || light_path.size() >= MAX_LIGHT_PATH_LEN - 1)
            continue;

        float pdf_dir_w;
        float3 sample_dir;
        bool is_specular;
        float4 bsdf = sample_material(isect.mat, isect.out_dir, isect.surf, rng, sample_dir, pdf_dir_w, is_specular); // TODO compute adjoint BRDF!
        float pdf_rev_w = pdf_dir_w; // TODO let material calculate this
        float cos_theta_o = dot(sample_dir, isect.surf.normal);

        BPTState s = states[i];
        if (is_specular) {
            s.dVCM = 0.0f;
            s.dVC = s.dVC * cos_theta_o;
        } else {
            s.dVC = cos_theta_o / pdf_dir_w * (s.dVC * pdf_rev_w + s.dVCM);
            s.dVCM = 1.0f / pdf_dir_w;
        }

        s.throughput *= bsdf * cos_theta_o / pdf_dir_w;
        s.path_length++;

        Ray ray {
            { isect.pos.x, isect.pos.y, isect.pos.z, offset },
            { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
        };

        rays_out.push(ray, s);
    }
}

void BidirPathTracer::connect_to_camera(const BPTState& light_state, const Intersection& isect, RayQueue<BPTState>& ray_out_shadow) {
    constexpr float offset = 0.001f;

    float3 dir_to_cam = cam_.pos() - isect.pos;

    if (dot(-dir_to_cam, cam_.dir()) < 0.0f)
        return; // Vertex is behind the camera.

    const float2 raster_pos = cam_.world_to_raster(isect.pos);

    BPTState state = light_state;
    state.pixel_id = cam_.raster_to_id(raster_pos);

    if (state.pixel_id < 0)
        return; // The point is outside the image plane.

    // Compute ray direction and distance.
    float dist_to_cam_sqr = lensqr(dir_to_cam);
    float dist_to_cam = sqrt(dist_to_cam_sqr);
    dir_to_cam = dir_to_cam / dist_to_cam;

    // Evaluate the bsdf.
    const float cos_theta_o = dot(isect.surf.normal, dir_to_cam);
    const float4 bsdf = evaluate_material(isect.mat, dir_to_cam, isect.surf, isect.out_dir);

    // Compute pdf.
    const float cos_theta_i = dot(cam_.dir(), -dir_to_cam);
    const float dist_pixel_to_cam = cam_.image_plane_dist() / cos_theta_i;
    const float img_to_solid_angle = dist_pixel_to_cam * dist_pixel_to_cam / cos_theta_i;
    const float img_to_surf = img_to_solid_angle * cos_theta_o / dist_to_cam_sqr;
    const float surf_to_img = 1.0f / img_to_surf;

    const float pdf = light_path_count_ * surf_to_img;

    state.throughput = float4(light_state.throughput * bsdf / pdf);

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { dir_to_cam.x, dir_to_cam.y, dir_to_cam.z, dist_to_cam - offset }
    };

    ray_out_shadow.push(ray, state);
}

void BidirPathTracer::process_camera_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& ray_out_shadow, Image& img) {
    const float offset = 0.01f;

    int ray_count = rays_in.size();
    BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0)
            continue;
    }
}

void BidirPathTracer::process_shadow_rays(RayQueue<BPTState>& rays_in, Image& img) {
    int ray_count = rays_in.size();
    const BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0) {
            assert(states[i].pixel_id >= 0 && states[i].pixel_id < img.width() * img.height() && "Write outside of image detected. (BPT::process_shadow_rays)");
            img.pixels()[states[i].pixel_id] += states[i].throughput;
        }
    }
}

} // namespace imba
