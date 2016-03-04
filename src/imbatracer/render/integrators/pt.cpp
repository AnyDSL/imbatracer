#include "pt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <random>
#include <chrono>

namespace imba {

constexpr float offset = 0.0001f;

inline void PathTracer::compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf) {
    RNG& rng = state.rng;

    // Generate the shadow ray (sample one point on one lightsource)
    const auto ls = scene_.lights[rng.random_int(0, scene_.lights.size())].get();
    const float pdf_inv_lightpick = scene_.lights.size();
    const auto sample = ls->sample_direct(isect.pos, rng);
    assert_normalized(sample.dir);

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample.dir.x, sample.dir.y, sample.dir.z, sample.distance - offset }
    };

    auto bsdf_value = bsdf->eval(isect.out_dir, sample.dir, BSDF_ALL);

    // Update the current state of this path.
    PTState s = state;
    s.throughput *= bsdf_value * fabsf(dot(isect.normal, sample.dir)) * sample.radiance * pdf_inv_lightpick;

    // Push the shadow ray into the queue.
    ray_out_shadow.push(ray, s);
}

inline void PathTracer::bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf) {
    RNG& rng = state.rng;

    const int max_recursion = 32; // prevent havoc
    if (state.bounces >= max_recursion)
        return;

    float rr_pdf;
    if (!russian_roulette(state.throughput, rng.random_float(), rr_pdf))
        return;

    float pdf;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, rng.random_float(), rng.random_float(), rng.random_float(),
                                   BSDF_ALL, sampled_flags, pdf);

    if (pdf <= 0.0001f)
        return;

    const float cos_term = fabsf(dot(isect.normal, sample_dir));

    PTState s = state;
    s.throughput *= bsdf_value * cos_term / (pdf * rr_pdf);
    s.bounces++;
    s.last_specular = sampled_flags & BSDF_SPECULAR;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };

    ray_out.push(ray, s);
}

void PathTracer::process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, Image& out) {
    PTState* states = ray_in.states();
    Hit* hits = ray_in.hits();
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count), [this, states, hits, rays, &ray_out, &ray_out_shadow, &out] (const tbb::blocked_range<size_t>& range) {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0)
                continue;

            // Create a thread_local memory arena that is used to store the BSDF objects
            // of all intersections that one thread processes.
            thread_local MemoryArena bsdf_mem_arena(512);
            bsdf_mem_arena.free_all();

            RNG& rng = states[i].rng;

            const auto isect = calculate_intersection(hits, rays, i);

            if (isect.mat->light()) {
                // If a light source is hit after a specular bounce or as the first intersection along the path, add its contribution.
                // otherwise the light has to be ignored because it was already sampled as direct illumination.
                if (states[i].bounces == 0 || states[i].last_specular) {
                    auto light_source = isect.mat->light();
                    float pdf_direct_a, pdf_emit_w;
                    auto li = light_source->radiance(isect.out_dir, pdf_direct_a, pdf_emit_w);

                    out.pixels()[states[i].pixel_id] += states[i].throughput * li;
                }
            }

            auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);
            compute_direct_illum(isect, states[i], ray_out_shadow, bsdf);

            bounce(isect, states[i], ray_out, bsdf);
        }
    });
}

void PathTracer::process_shadow_rays(RayQueue<PTState>& ray_in, Image& out) {
    PTState* states = ray_in.states();
    Hit* hits = ray_in.hits();
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count), [states, hits, &out] (const tbb::blocked_range<size_t>& range) {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0) {
                // The shadow ray hit the light source. Multiply the contribution of the light by the
                // current throughput of the path (as stored in the state of the shadow ray)
                float4 color = states[i].throughput;
                // Add contribution to the pixel which this ray belongs to.
                out.pixels()[states[i].pixel_id] += color;
            }
        }
    });
}

void PathTracer::render(Image& out) {
    scheduler_.run_iteration(out, this, &PathTracer::process_shadow_rays, &PathTracer::process_primary_rays);
/*

    // Create the initial set of camera rays.
    auto camera = ray_gen_;
    camera.start_frame();

    int in_queue = 0;
    int out_queue = 1;

    static int64_t traversal_time = 0;
    static int64_t traversal_shadow_time = 0;
    static int64_t shading_time = 0;
    static int64_t raygen_time = 0;
    static int64_t shadow_process_time = 0;
    static int64_t total_rays = 0;
    static int64_t total_rays_shadow = 0;
    static int64_t traversal_calls = 0;

    while(true) {
        auto t0 = std::chrono::high_resolution_clock::now();

        camera.fill_queue(primary_rays_[in_queue]);

        auto t1 = std::chrono::high_resolution_clock::now();
        raygen_time += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        if (primary_rays_[in_queue].size() <= 0)
            break;

        total_rays += primary_rays_[in_queue].size();

        t0 = std::chrono::high_resolution_clock::now();

        primary_rays_[in_queue].traverse(scene_);

        t1 = std::chrono::high_resolution_clock::now();
        traversal_time += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        traversal_calls++;

        t0 = std::chrono::high_resolution_clock::now();

        process_primary_rays(primary_rays_[in_queue], primary_rays_[out_queue], shadow_rays_, out);
        primary_rays_[in_queue].clear();

        t1 = std::chrono::high_resolution_clock::now();
        shading_time += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        // Processing primary rays creates new primary rays and some shadow rays.
        if (shadow_rays_.size() > 0) {
            t0 = std::chrono::high_resolution_clock::now();

            shadow_rays_.traverse_occluded(scene_);

            total_rays_shadow += shadow_rays_.size();

            t1 = std::chrono::high_resolution_clock::now();
            traversal_shadow_time += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

            t0 = std::chrono::high_resolution_clock::now();

            process_shadow_rays(shadow_rays_, out);
            shadow_rays_.clear();

            t1 = std::chrono::high_resolution_clock::now();
            shadow_process_time += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        }

        std::swap(in_queue, out_queue);
    }

    primary_rays_[in_queue].clear();

    static int ctr = 0;
    static int step = 4;
    if (++ctr % step == 0) {
        printf("%d primary %d shadow, traversal: %dms, shadow ray traversal: %dms, shading: %dms, ray generation: %dms, writing pixels: %dms\n",
                total_rays / step, total_rays_shadow / step, traversal_time / step, traversal_shadow_time / step, shading_time / step, raygen_time / step, shadow_process_time / step);

        printf("%d calls to traversal, %d rays on average\n\n", traversal_calls / step, total_rays / traversal_calls);

        traversal_time = 0;
        shading_time = 0;
        traversal_shadow_time = 0;
        raygen_time = 0;
        shadow_process_time = 0;

        total_rays = 0;
        total_rays_shadow = 0;

        traversal_calls = 0;
    }*/
}

} // namespace imba
