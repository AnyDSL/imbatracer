#include "bpt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <cmath>

namespace imba {

void BidirPathTracer::process_light_rays(RayQueue<LightRayState>& rays_in, RayQueue<LightRayState>& rays_out) {
    constexpr float offset = 0.01f;

    int ray_count = rays_in.size();
    LightRayState* states = rays_in.states();
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

        LightRayState s = states[i];
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

void BidirPathTracer::process_primary_rays(RayQueue<BPTState>& rays_in, RayQueue<BPTState>& rays_out, RayQueue<BPTState>& ray_out_shadow, Image& img) {
    const float offset = 0.01f;

    int ray_count = rays_in.size();
    BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0)
            continue;

       /* RNG& rng = states[i].rng;

        Intersection isect = calculate_intersection(hits, rays, i);

        auto& light_path = light_paths_[states[i].pixel_id][states[i].sample_id];

        if (isect.mat->kind == Material::emissive) {
            // If an emissive object is hit after a specular bounce or as the first intersection along the path, add its contribution.
            // otherwise the light has to be ignored because it was already sampled as direct illumination.
            if (states[i].bounces == 0) {
                EmissiveMaterial* em = static_cast<EmissiveMaterial*>(isect.mat);
                float4 color = em->color();

                // Add contribution to the pixel which this ray belongs to.
                img.pixels()[states[i].pixel_id] += color;
            } else if (states[i].last_specular) {
                EmissiveMaterial* em = static_cast<EmissiveMaterial*>(isect.mat);

                float cos_light = fabsf(dot(isect.surf.normal, -1.0f * isect.out_dir));
                if (cos_light < 1.0f) {  // Only add contribution from the side of the light that the normal is on.
                    float4 color = states[i].contribution * em->color();

                    // Add contribution to the pixel which this ray belongs to.
                    img.pixels()[states[i].pixel_id] += color;
                }
            }

            // Do not continue the path after hitting a light source.
            continue;
        }

        //compute_direct_illum(rng, isect, states[i], ray_out_shadow);

        {
            int x = states[i].rng.random_int(0, scene_.lights.size() - 1);
            auto& l = scene_.lights[x];
            Light::LightRaySample sample = l->sample(states[i].rng);

            float3 connect_dir = sample.pos - isect.pos;
            const float distance = length(connect_dir);
            connect_dir = connect_dir / distance;

            if (dot(connect_dir, isect.surf.normal) * dot(isect.out_dir, isect.surf.normal) < 0.0f
                || dot(-connect_dir, sample.normal) < 0.0f)
                goto skip_di;

            const float cos_term_eye = fabsf(dot(connect_dir, isect.surf.normal));
            const float4 brdf_eye = evaluate_material(isect.mat, isect.out_dir, isect.surf, connect_dir);

            const float cos_term_light = fabsf(dot(connect_dir, sample.normal));
            const float4 brdf_light = sample.intensity;

            const float geom_factor = cos_term_eye * cos_term_light / (distance * distance);

            const float4 connect_contribution = brdf_eye * brdf_light * geom_factor;
            const float weight = static_cast<float>(states[i].bounces + 2);

            BPTState s = states[i];
            // We split the unweighted contribution of a path in three parts as in Veach [1994] p. 304.
            s.contribution = 1.0f * connect_contribution * states[i].contribution / weight;

            Ray ray {
                { isect.pos.x, isect.pos.y, isect.pos.z, offset },
                { connect_dir.x, connect_dir.y, connect_dir.z, distance - offset }
            };

            shadow_rays_.push(ray, s);
        }
skip_di:

        // Connect the hitpoint to the light path.
        for (int k = 0; k < n_vertices; ++k) {
            float3 connect_dir = sample_path[k].pos - isect.pos;
            const float distance = length(connect_dir);
            connect_dir = connect_dir / distance;

            if (dot(connect_dir, isect.surf.normal) * dot(isect.out_dir, isect.surf.normal) < 0.0f
                || dot(-connect_dir, sample_path[k].surf.normal) * dot(sample_path[k].in_dir, sample_path[k].surf.normal) < 0.0f)
                continue;

            const float cos_term_eye = fabsf(dot(connect_dir, isect.surf.normal));
            const float4 brdf_eye = evaluate_material(isect.mat, isect.out_dir, isect.surf, connect_dir);

            const float cos_term_light = fabsf(dot(-connect_dir, sample_path[k].surf.normal));
            const float4 brdf_light = evaluate_material(sample_path[k].mat, -connect_dir, sample_path[k].surf, sample_path[k].in_dir);

            const float geom_factor = cos_term_eye * cos_term_light / (distance * distance);

            const float4 connect_contribution = brdf_eye * brdf_light * geom_factor;

            const float weight = static_cast<float>(states[i].bounces + 1 + k);
            //printf("%f\n", weight);

            BPTState s = states[i];
            // We split the unweighted contribution of a path in three parts as in Veach [1994] p. 304.
            s.contribution = sample_path[k].contribution * connect_contribution * states[i].contribution;

            Ray ray {
                { isect.pos.x, isect.pos.y, isect.pos.z, offset },
                { connect_dir.x, connect_dir.y, connect_dir.z, distance - offset }
            };

            shadow_rays_.push(ray, s);
        }

        // continue the camera path
        const float4 srgb(0.2126f, 0.7152f, 0.0722f, 0.0f);
        const float kill_prob = dot(states[i].contribution, srgb) * 100.0f;

        const float rrprob = std::min(1.0f, kill_prob);
        const float u_rr = rng.random_float();
        const int max_recursion = 32; // prevent havoc
        if (u_rr > rrprob || states[i].bounces >= max_recursion)
            continue;

        // Sample the brdf
        float pdf;
        float3 sample_dir;
        bool specular;
        const float4 brdf = sample_material(isect.mat, isect.out_dir, isect.surf, rng, sample_dir, pdf, specular);
        const float cos_term = fabsf(dot(isect.surf.normal, sample_dir));

        BPTState s = states[i];
        s.contribution = s.contribution * brdf * (cos_term / (rrprob * pdf));
        s.bounces++;
        s.last_specular = specular;

        Ray ray {
            { isect.pos.x, isect.pos.y, isect.pos.z, offset },
            { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
        };

        rays_out.push(ray, s);*/
    }
}

void BidirPathTracer::process_shadow_rays(RayQueue<BPTState>& rays_in, Image& img) {
    int ray_count = rays_in.size();
    const BPTState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0) {
            img.pixels()[states[i].pixel_id] += states[i].throughput;
        }
    }
}

void BidirPathTracer::trace_light_paths() {
    light_sampler_.start_frame();

    int in_queue = 0;
    int out_queue = 1;

    while(true) {
        light_sampler_.fill_queue(light_rays_[in_queue]);

        if (light_rays_[in_queue].size() <= 0)
            break;

        light_rays_[in_queue].traverse(scene_);
        process_light_rays(light_rays_[in_queue], light_rays_[out_queue]);
        light_rays_[in_queue].clear();

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
        process_primary_rays(primary_rays_[in_queue], primary_rays_[out_queue], shadow_rays_, img);
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

void BidirPathTracer::reset_buffers() {
    for (auto& p : light_paths_) {
        for (auto& s : p) s.clear();
    }
}

void BidirPathTracer::render(Image& img) {
    reset_buffers();

    trace_light_paths();
    trace_camera_paths(img);
}

} // namespace imba
