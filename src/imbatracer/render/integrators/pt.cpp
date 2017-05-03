#include "pt.h"
#include "../../core/rgb.h"
#include "../../core/common.h"
#include "../random.h"

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <cfloat>
#include <cassert>

namespace imba {

using ThreadLocalMemArena = tbb::enumerable_thread_specific<MemoryArena, tbb::cache_aligned_allocator<MemoryArena>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

void PathTracer::compute_direct_illum(const Intersection& isect, PTState& state, RayQueue<ShadowState>& ray_out_shadow, BSDF* bsdf) {
    // Generate the shadow ray (sample one point on one lightsource)
    const auto& ls = scene_.light(state.rng.random_int(0, scene_.light_count()));
    const float pdf_lightpick = 1.0f / scene_.light_count();
    const auto sample = ls->sample_direct(isect.pos, state.rng);

    const auto bsdf_value = bsdf->eval(isect.out_dir, sample.dir, BSDF_ALL);

    const float pdf_hit = bsdf->pdf(isect.out_dir, sample.dir);
    const float pdf_di  = pdf_lightpick * sample.pdf_direct_w;
    const float mis_weight = ls->is_delta() ? 1.0f : pdf_di / (pdf_di + pdf_hit);

    if (pdf_hit == 0.0f || pdf_di == 0.0f)
        return;

    // The contribution is stored in the state of the shadow ray and added, if the shadow ray does not intersect anything.
    ShadowState s;
    s.throughput = state.throughput * bsdf_value * fabsf(dot(isect.normal, sample.dir)) * sample.radiance * mis_weight / pdf_lightpick;
    s.pixel_id   = state.pixel_id;

    const float offset = 1e-3f * sample.distance;
    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample.dir.x, sample.dir.y, sample.dir.z, sample.distance - offset }
    };

    ray_out_shadow.push(ray, s);
}

void PathTracer::bounce(const Intersection& isect, PTState& state_out, Ray& ray_out, BSDF* bsdf, float offset) {
    // Terminate the path if it is too long or with russian roulette.
    if (state_out.bounces + 1 >= max_path_len_) {// Path length includes the vertices on the camera and the light.
        terminate_path(state_out);
        return;
    }

    float rr_pdf;
    if (!russian_roulette(state_out.throughput, state_out.rng.random_float(), rr_pdf)) {
        terminate_path(state_out);
        return;
    }

    float pdf;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    const auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, state_out.rng, BSDF_ALL, sampled_flags, pdf);

    if (pdf == 0.0f || sampled_flags == BSDF_NONE || is_black(bsdf_value)) {
        terminate_path(state_out);
        return;
    }

    const float cos_term = fabsf(dot(isect.normal, sample_dir));

    state_out.throughput *= bsdf_value * cos_term / (pdf * rr_pdf);
    state_out.bounces++;
    state_out.last_specular = sampled_flags & BSDF_SPECULAR;
    state_out.last_pdf = pdf;

    ray_out = Ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };
}

void PathTracer::process_primary_rays(RayQueue<PTState>& ray_in, RayQueue<ShadowState>& ray_out_shadow, AtomicImage& res_img) {
    // Compact and sort the input hits.
    int hit_count = ray_in.compact_hits();
    ray_in.sort_by_material([this](const Hit& hit){
            const Mesh::Instance& inst = scene_.instance(hit.inst_id);
            const Mesh& mesh = scene_.mesh(inst.id);
            const int local_tri_id = scene_.local_tri_id(hit.tri_id, inst.id);
            const int m = mesh.indices()[local_tri_id * 4 + 3];
            return m;
        },
        scene_.material_count(), hit_count);

    // Process all rays that hit nothing, if there is an environment map.
    if (scene_.env_map() != nullptr) {
        tbb::parallel_for(tbb::blocked_range<int>(hit_count, ray_in.size()),
            [&] (const tbb::blocked_range<int>& range)
        {
            for (auto i = range.begin(); i != range.end(); ++i) {
                PTState& state = ray_in.state(i);
                float3 out_dir(ray_in.ray(i).dir.x, ray_in.ray(i).dir.y, ray_in.ray(i).dir.z);
                out_dir = normalize(out_dir);

                float pdf_direct_w, pdf_emit_w;
                const auto li = scene_.env_map()->radiance(out_dir, pdf_direct_w, pdf_emit_w);

                const float pdf_lightpick = 1.0f / scene_.light_count();
                const float pdf_di  = pdf_direct_w * pdf_lightpick;
                const float pdf_hit = state.last_pdf;
                const float mis_weight = (state.bounces == 0 || state.last_specular) ? 1.0f
                                         : (pdf_hit / (pdf_hit + pdf_di));

                add_contribution(res_img, state.pixel_id, state.throughput * li * mis_weight);
            }
        });
    }

    // Shrink the queue to only contain valid hits.
    ray_in.shrink(hit_count);

    // Process all hits, creating continuation and shadow rays.
    tbb::parallel_for(tbb::blocked_range<int>(0, ray_in.size()),
        [&] (const tbb::blocked_range<int>& range)
    {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (auto i = range.begin(); i != range.end(); ++i) {
            bsdf_mem_arena.free_all();
            PTState& state = ray_in.state(i);
            const auto isect = calculate_intersection(scene_, ray_in.hit(i), ray_in.ray(i));
            const float offset = 1e-3f * ray_in.hit(i).tmax;

            if (auto emit = isect.mat->emitter()) {
                float pdf_direct_a, pdf_emit_w;
                const auto li = emit->radiance(isect.out_dir, isect.geom_normal, pdf_direct_a, pdf_emit_w);

                float pdf_di = pdf_direct_a / scene_.light_count();

                // convert pdf from area measure to solid angle measure
                const float d_sqr = ray_in.hit(i).tmax * ray_in.hit(i).tmax;
                const float cos_light = dot(isect.normal, isect.out_dir);
                pdf_di *= d_sqr / cos_light;

                const float mis_weight = (state.bounces == 0 || state.last_specular) ? 1.0f
                                         : state.last_pdf / (state.last_pdf + pdf_di);

                add_contribution(res_img, state.pixel_id, state.throughput * li * mis_weight);

                terminate_path(state);
                continue;
            }

            const auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);
            compute_direct_illum(isect, state, ray_out_shadow, bsdf);
            bounce(isect, state, ray_in.ray(i), bsdf, offset);
        }
    });

    ray_in.compact_rays();
}

void PathTracer::render(AtomicImage& out) {
    scheduler_.run_iteration(out,
        [this] (RayQueue<ShadowState>& ray_in, AtomicImage& out) { process_shadow_rays(ray_in, out); },
        [this] (RayQueue<PTState>& ray_in, RayQueue<ShadowState>& ray_out_shadow, AtomicImage& out) {
            process_primary_rays(ray_in, ray_out_shadow, out);
        },
        [this] (int x, int y, ::Ray& ray_out, PTState& state_out) {
            const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
            const float sample_y = static_cast<float>(y) + state_out.rng.random_float();

            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = rgb(1.0f);
            state_out.bounces = 0;
            state_out.last_specular = false;
        });
}

} // namespace imba
