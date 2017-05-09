#include "light_vertices.h"

#include "scheduling/ray_queue.h"

#include <tbb/enumerable_thread_specific.h>

namespace imba {

using ThreadLocalMemArena = tbb::enumerable_thread_specific<MemoryArena, tbb::cache_aligned_allocator<MemoryArena>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

struct ProbePathState : RayState {
    rgb throughput;
};

void imba::LightVertices::compute_cache_size(const Scene& scene, bool use_gpu) {
    // Trace a couple of light paths into the scene and calculate the average number of
    // vertices that would have been stored by these paths.

    // Setup the queues. We need two: one for the current rays/hits and one for continuation rays.
    RayQueue<ProbePathState>* queues[2] = {
        new RayQueue<ProbePathState>(LIGHT_PATH_LEN_PROBES, use_gpu),
        new RayQueue<ProbePathState>(LIGHT_PATH_LEN_PROBES, use_gpu)
    };

    int in_q = 0;
    int out_q = 1;

    // Fill the first queue with the initial light rays.
    for (int i = 0; i < LIGHT_PATH_LEN_PROBES; ++i) {
        Ray ray_out;
        ProbePathState state_out;
        state_out.pixel_id = i;

        static std::random_device rd;
        state_out.rng = RNG(rd());

        // randomly choose one light source to sample
        auto& l = scene.lights()[state_out.rng.random_int(0, scene.light_count())];
        float pdf_lightpick = 1.0f / scene.light_count();

        Light::EmitSample sample = l->sample_emit(state_out.rng);
        ray_out.org.x = sample.pos.x;
        ray_out.org.y = sample.pos.y;
        ray_out.org.z = sample.pos.z;
        ray_out.org.w = 1e-3f;

        ray_out.dir.x = sample.dir.x;
        ray_out.dir.y = sample.dir.y;
        ray_out.dir.z = sample.dir.z;
        ray_out.dir.w = FLT_MAX;

        state_out.throughput = sample.radiance / pdf_lightpick;

        queues[in_q]->push(ray_out, state_out);
    }

    // Trace the light paths until they are (almost) all terminated.
    // Count the vertices they would store.
    std::atomic<int> vertex_count(0);
    while (queues[in_q]->size() > 256) {
        if (use_gpu)
            queues[in_q]->traverse_gpu(scene.traversal_data_gpu());
        else
            queues[in_q]->traverse_cpu(scene.traversal_data_cpu());

        // Process hitpoints and bounce or terminate paths.
        const int ray_count = queues[in_q]->size();
        ProbePathState* states    = queues[in_q]->states();
        const Hit* hits     = queues[in_q]->hits();
        const Ray* rays     = queues[in_q]->rays();
        auto& rays_out      = *queues[out_q];
        tbb::parallel_for(tbb::blocked_range<int>(0, ray_count),
                          [&] (const tbb::blocked_range<int>& range) {
            auto& bsdf_mem_arena = bsdf_memory_arenas.local();

            for (auto i = range.begin(); i != range.end(); ++i) {
                float rr_pdf;
                RNG& rng = states[i].rng;
                if (hits[i].tri_id < 0 || !russian_roulette(states[i].throughput, rng.random_float(), rr_pdf))
                    continue;

                bsdf_mem_arena.free_all();

                const auto isect = calculate_intersection(scene, hits[i], rays[i]);
                auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena, true);

                if (!isect.mat->is_specular())
                    ++vertex_count; // we would store a vertex at this position

                float pdf_dir_w;
                float3 sample_dir;
                BxDFFlags sampled_flags;
                auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, rng, BSDF_ALL, sampled_flags, pdf_dir_w);

                if (sampled_flags == 0 || pdf_dir_w == 0.0f || is_black(bsdf_value))
                    continue;

                const float cos_theta_i = fabsf(dot(isect.normal, sample_dir));

                ProbePathState s = states[i];
                s.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);

                const float offset = hits[i].tmax * 1e-3f;

                Ray ray {
                    { isect.pos.x, isect.pos.y, isect.pos.z, offset },
                    { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
                };

                rays_out.push(ray, s);
            }
        });

        queues[in_q]->clear();

        std::swap(in_q, out_q);
    }

    // Release the queues.
    delete queues[0];
    delete queues[1];

    const float avg_len = static_cast<float>(vertex_count) / static_cast<float>(LIGHT_PATH_LEN_PROBES);

    const float margin = (path_count_ < LIGHT_PATH_LEN_PROBES / 10) ? 10.0f : 1.1f;
    const int vc_size = margin * std::ceil(avg_len) * path_count_;

    cache_.resize(vc_size);
}

} // namespace imba
