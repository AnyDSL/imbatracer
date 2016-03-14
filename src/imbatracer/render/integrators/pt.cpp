#include "pt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <tbb/enumerable_thread_specific.h>

#include <cfloat>
#include <cassert>
#include <random>
#include <chrono>

namespace imba {

constexpr float offset = 0.0001f;

using ThreadLocalMemArena = tbb::enumerable_thread_specific<MemoryArena, tbb::cache_aligned_allocator<MemoryArena>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

void PathTracer::compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf) {
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

void PathTracer::bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf) {
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

void PathTracer::process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, AtomicImage& out) {
    PTState* states = ray_in.states();
    Hit* hits = ray_in.hits();
    Ray* rays = ray_in.rays();
    int ray_count = ray_in.size();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count), [this, states, hits, rays, &ray_out, &ray_out_shadow, &out] (const tbb::blocked_range<size_t>& range) {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0)
                continue;

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

void PathTracer::process_shadow_rays(RayQueue<PTState>& ray_in, AtomicImage& out) {
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

void PathTracer::render(AtomicImage& out) {
    scheduler_.run_iteration(out, this, &PathTracer::process_shadow_rays, &PathTracer::process_primary_rays,
        [this] (int x, int y, ::Ray& ray_out, PTState& state_out) {
            float u1 = state_out.rng.random_float();
            float u2 = state_out.rng.random_float();
            const float sample_x = static_cast<float>(x) + u1;
            const float sample_y = static_cast<float>(y) + u2;

            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = float4(1.0f);
            state_out.bounces = 0;
            state_out.last_specular = false;
        });
}

} // namespace imba

