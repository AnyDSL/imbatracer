#include "pt.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <tbb/enumerable_thread_specific.h>

#include <cfloat>
#include <cassert>

namespace imba {

constexpr float offset = 0.0001f;

using ThreadLocalMemArena = tbb::enumerable_thread_specific<MemoryArena, tbb::cache_aligned_allocator<MemoryArena>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

void PathTracer::compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out_shadow, BSDF* bsdf) {
    // Generate the shadow ray (sample one point on one lightsource)
    const auto ls = scene_.lights[state.rng.random_int(0, scene_.lights.size())].get();
    const float pdf_lightpick_inv = scene_.lights.size();
    const auto sample = ls->sample_direct(isect.pos, state.rng);

    const auto bsdf_value = bsdf->eval(isect.out_dir, sample.dir, BSDF_ALL);

    // The contribution is stored in the state of the shadow ray and added, if the shadow ray does not intersect anything.
    PTState s = state;
    s.throughput *= bsdf_value * fabsf(dot(isect.normal, sample.dir)) * sample.radiance * pdf_lightpick_inv;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample.dir.x, sample.dir.y, sample.dir.z, sample.distance - offset }
    };

    ray_out_shadow.push(ray, s);
}

void PathTracer::bounce(const Intersection& isect, PTState& state, RayQueue<PTState>& ray_out, BSDF* bsdf) {
    // Terminate the path if it is too long or with russian roulette.
    if (state.bounces + 1 >= max_path_len_) // Path length includes the vertices on the camera and the light.
        return;

    float rr_pdf;
    if (!russian_roulette(state.throughput, state.rng.random_float(), rr_pdf))
        return;

    float pdf;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    const auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, state.rng, BSDF_ALL, sampled_flags, pdf);

    if (pdf == 0.0f || sampled_flags == BSDF_NONE || is_black(bsdf_value))
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

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_in.size()),
        [this, states, hits, rays, &ray_out, &ray_out_shadow, &out] (const tbb::blocked_range<size_t>& range)
    {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0)
                continue;

            bsdf_mem_arena.free_all();

            const auto isect = calculate_intersection(scene_, hits, rays, i);

            if (isect.mat->light()) {
                // If a light source is hit after a specular bounce or as the first intersection along the path, add its contribution.
                // otherwise the light has to be ignored because it was already sampled as direct illumination.
                if (states[i].bounces == 0 || states[i].last_specular) {
                    const auto light_source = isect.mat->light();
                    float pdf_direct_a, pdf_emit_w;
                    const auto li = light_source->radiance(isect.out_dir, pdf_direct_a, pdf_emit_w);

                    out.pixels()[states[i].pixel_id] += states[i].throughput * li;
                }
            }

            const auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);
            compute_direct_illum(isect, states[i], ray_out_shadow, bsdf);
            bounce(isect, states[i], ray_out, bsdf);
        }
    });
}

void PathTracer::process_shadow_rays(RayQueue<PTState>& ray_in, AtomicImage& out) {
    PTState* states = ray_in.states();
    Hit* hits = ray_in.hits();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_in.size()),
        [states, hits, &out] (const tbb::blocked_range<size_t>& range)
    {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0) {
                // Nothing was hit, the light source is visible.
                out.pixels()[states[i].pixel_id] += states[i].throughput;
            }
        }
    });
}

void PathTracer::render(AtomicImage& out) {
    scheduler_.run_iteration(out,
        [this] (RayQueue<PTState>& ray_in, AtomicImage& out) { process_shadow_rays(ray_in, out); },
        [this] (RayQueue<PTState>& ray_in, RayQueue<PTState>& ray_out, RayQueue<PTState>& ray_out_shadow, AtomicImage& out) {
            process_primary_rays(ray_in, ray_out, ray_out_shadow, out);
        },
        [this] (int x, int y, ::Ray& ray_out, PTState& state_out) {
            const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
            const float sample_y = static_cast<float>(y) + state_out.rng.random_float();

            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = float4(1.0f);
            state_out.bounces = 0;
            state_out.last_specular = false;
        });
}

} // namespace imba

