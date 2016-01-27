#include "pt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <random>

namespace imba {

inline void PathTracer::compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow) {
    const float offset = 0.001f;

    RNG& rng = state.rng;

    // Generate the shadow ray (sample one point on one lightsource)
    const auto ls = scene_.lights[rng.random_int(0, scene_.lights.size())].get();
    const float pdf = scene_.lights.size();
    const auto sample = ls->sample_direct(isect.pos, rng);
    assert_normalized(sample.dir);

    // Ensure that the incoming and outgoing directions are on the same side of the surface.
    if (dot(isect.geom_normal, sample.dir) *
        dot(isect.geom_normal, isect.out_dir) <= 0.0f)
        return;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample.dir.x, sample.dir.y, sample.dir.z, sample.distance - offset }
    };

    // Compute the values stored in the ray state.
    float pdf_dir, pdf_rev;
    const float4 brdf = evaluate_material(isect.mat, isect, sample.dir, false, pdf_dir, pdf_rev);

    // Update the current state of this path.
    PTState s = state;
    s.throughput *= brdf * sample.radiance * pdf;

    // Push the shadow ray into the queue.
    ray_out_shadow.push(ray, s);
}

void PathTracer::process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, Image& out) {
    PTState* states = ray_in.states();
    Hit* hits = ray_in.hits();
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size();

    const float offset = 0.001f;

    #pragma omp parallel for
    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0)
            continue;

        RNG& rng = states[i].rng;

        auto isect = calculate_intersection(hits, rays, i);

        if (isect.mat->kind == Material::emissive) {
            // If an emissive object is hit after a specular bounce or as the first intersection along the path, add its contribution.
            // otherwise the light has to be ignored because it was already sampled as direct illumination.
            if (states[i].bounces == 0) {
                EmissiveMaterial* em = static_cast<EmissiveMaterial*>(isect.mat);
                float4 color = em->color();

                // Add contribution to the pixel which this ray belongs to.
                out.pixels()[states[i].pixel_id] += color;
            } else if (states[i].last_specular) {
                EmissiveMaterial* em = static_cast<EmissiveMaterial*>(isect.mat);

                float cos_light = fabsf(dot(isect.normal, -1.0f * isect.out_dir));
                if (cos_light < 1.0f) {  // Only add contribution from the side of the light that the normal is on.
                    float4 color = states[i].throughput * em->color();

                    // Add contribution to the pixel which this ray belongs to.
                    out.pixels()[states[i].pixel_id] += color;
                }
            }

            // Do not continue the path after hitting a light source.
            continue;
        }

        compute_direct_illum(isect, states[i], ray_out_shadow);

        // Continue the path using russian roulette.
        const float4 srgb(0.2126f, 0.7152f, 0.0722f, 0.0f);
        const float kill_prob = dot(states[i].throughput, srgb) * 100.0f;
        const float rrprob = std::min(1.0f, kill_prob);
        const float u_rr = rng.random_float();
        const int max_recursion = 32; // prevent havoc
        if (u_rr < rrprob && states[i].bounces < max_recursion) {
            // sample brdf
            float pdf;
            float3 sample_dir;
            bool specular;

            const float4 brdf = sample_material(isect.mat, isect, rng, sample_dir, false, pdf, specular);
            const float cos_term = fabsf(dot(isect.normal, sample_dir));

            PTState s = states[i];
            s.throughput *= brdf / rrprob;

            s.bounces++;
            s.last_specular = specular;

            Ray ray {
                { isect.pos.x, isect.pos.y, isect.pos.z, offset },
                { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
            };

            ray_out.push(ray, s);
        }
    }
}

void PathTracer::process_shadow_rays(RayQueue<PTState>& ray_in, Image& out) {
    PTState* states = ray_in.states();
    Hit* hits = ray_in.hits();
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size();

    #pragma omp parallel for
    for (int i = 0; i < ray_count; ++i) {
        if (hits[i].tri_id < 0) {
            // The shadow ray hit the light source. Multiply the contribution of the light by the
            // current throughput of the path (as stored in the state of the shadow ray)
            float4 color = states[i].throughput;
            // Add contribution to the pixel which this ray belongs to.
            out.pixels()[states[i].pixel_id] += color;
        }
    }
}

void PathTracer::render(Image& out) {
    // Create the initial set of camera rays.
    auto camera = ray_gen_;
    camera.start_frame();

    int in_queue = 0;
    int out_queue = 1;

    while(true) {
        camera.fill_queue(primary_rays_[in_queue]);

        if (primary_rays_[in_queue].size() <= 0)
            break;

        primary_rays_[in_queue].traverse(scene_);
        process_primary_rays(primary_rays_[in_queue], primary_rays_[out_queue], shadow_rays_, out);
        primary_rays_[in_queue].clear();

        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays_.size() > 0) {
            shadow_rays_.traverse_occluded(scene_);
            process_shadow_rays(shadow_rays_, out);
            shadow_rays_.clear();
        }

        std::swap(in_queue, out_queue);
    }
}

} // namespace imba
