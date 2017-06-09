#include "imbatracer/render/integrators/deferred_vertices.h"
#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"
#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/scheduling/deferred_scheduler.h"

#include <tbb/enumerable_thread_specific.h>

namespace imba {

using ThreadLocalMemArena =
    tbb::enumerable_thread_specific<MemoryArena,
        tbb::cache_aligned_allocator<MemoryArena>,
        tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;


struct ProbeState : RayState {
    rgb throughput;
};

void bounce(const Scene& scene, Ray& r, Hit& h, ProbeState& s, std::atomic<int>& vertex_count) {
    auto& bsdf_mem_arena = bsdf_memory_arenas.local();
    bsdf_mem_arena.free_all();

    const auto isect = calculate_intersection(scene, h, r);
    auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena, true);

    if (!isect.mat->is_specular())
        ++vertex_count; // we would store a vertex at this position

    float rr_pdf;
    if (!russian_roulette(s.throughput, s.rng.random_float(), rr_pdf))
        return;

    float pdf_dir_w;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, s.rng, BSDF_ALL, sampled_flags, pdf_dir_w);

    if (sampled_flags == 0 || pdf_dir_w == 0.0f || is_black(bsdf_value))
        return;

    const float cos_theta_i = fabsf(dot(isect.normal, sample_dir));

    s.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);

    const float offset = h.tmax * 1e-3f;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };

    r = ray;
}

int estimate_light_path_len(const Scene& scene, bool use_gpu, int probes) {
    UniformLightTileGen<ProbeState> tile_gen(scene.light_count(), probes, 512 * 512);
    DeferredScheduler<ProbeState> scheduler(&scene, 4, 256 * 256, use_gpu, tile_gen.sizeof_ray_gen());

    std::atomic<int> vertex_count(0);
    scheduler.run_iteration(&tile_gen,
        nullptr,
        [&vertex_count, &scene] (Ray& r, Hit& h, ProbeState& s) {
            bounce(scene, r, h, s, vertex_count);
        },
        [&vertex_count, &scene] (int ray_id, int light_id, ::Ray& ray, ProbeState& state) -> bool {
            auto& l = scene.light(light_id);
            // TODO: this pdf depends on the LightTileGen used!
            float pdf_lightpick = 1.0f / scene.light_count();

            Light::EmitSample sample = l->sample_emit(state.rng);
            ray.org.x = sample.pos.x;
            ray.org.y = sample.pos.y;
            ray.org.z = sample.pos.z;
            ray.org.w = 1e-4f;

            ray.dir.x = sample.dir.x;
            ray.dir.y = sample.dir.y;
            ray.dir.z = sample.dir.z;
            ray.dir.w = FLT_MAX;

            state.throughput = sample.radiance / pdf_lightpick;

            vertex_count++;

            return true;
        });

    const float avg_len = static_cast<float>(vertex_count) / static_cast<float>(probes);
    return std::ceil(avg_len);
}

int estimate_cam_path_len(const Scene& scene, const PerspectiveCamera& cam, bool use_gpu, int probes) {
    DefaultTileGen<ProbeState> tile_gen(cam.width(), cam.height(), probes, 256);
    DeferredScheduler<ProbeState> scheduler(&scene, 4, 256 * 256, use_gpu, tile_gen.sizeof_ray_gen());

    std::atomic<int> vertex_count(0);
    scheduler.run_iteration(&tile_gen,
        nullptr,
        [&vertex_count, &scene] (Ray& r, Hit& h, ProbeState& s) {
            bounce(scene, r, h, s, vertex_count);
        },
        [&vertex_count, &cam] (int x, int y, ::Ray& ray, ProbeState& state) -> bool {
            const float sample_x = static_cast<float>(x) + state.rng.random_float();
            const float sample_y = static_cast<float>(y) + state.rng.random_float();

            ray = cam.generate_ray(sample_x, sample_y);

            state.throughput = rgb(1.0f);

            return true;
        });

    const float avg_len = static_cast<float>(vertex_count) / static_cast<float>(cam.width() * cam.height() * probes);
    return std::ceil(avg_len);
}

} // namespace imba