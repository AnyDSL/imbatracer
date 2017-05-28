#include "imbatracer/render/integrators/deferred_vcm.h"

#include <tbb/enumerable_thread_specific.h>

namespace imba {

// Thread-local storage for BSDF objects.
using ThreadLocalMemArena =
    tbb::enumerable_thread_specific<MemoryArena,
        tbb::cache_aligned_allocator<MemoryArena>,
        tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

void DeferredVCM::render(AtomicImage& img) {
    trace_camera_paths(img);
    trace_light_paths(img);

    direct_illum(img);
    connect_to_camera(img);
    connect(img);
    merge(img);
}

void DeferredVCM::trace_camera_paths(AtomicImage& img) {
    DeferredScheduler<State>::ShadeEmptyFn empty;
    if (scene_.env_map()) {
        empty = [this] (Ray& r, State& s, AtomicImage& img) {
            process_camera_empties(r, s, img);
        };
    }

    scheduler_.run_iteration(&camera_tile_gen_, img,
        empty,
        [this] (Ray& r, Hit& h, State& s, AtomicImage& img) {
            process_camera_hits(r, h, s, img);
        },
        [this] (int x, int y, Ray& ray, State& state) {
            // Sample a ray from the camera.
            const float sample_x = static_cast<float>(x) + state.rng.random_float();
            const float sample_y = static_cast<float>(y) + state.rng.random_float();

            ray = cam_.generate_ray(sample_x, sample_y);

            state.throughput = rgb(1.0f);
            state.path_length = 1;

            init_camera_mis(ray, state);
        });
}

void DeferredVCM::trace_light_paths(AtomicImage& img) {
    scheduler_.run_iteration(&light_tile_gen_, img,
        nullptr,
        [this] (Ray& r, Hit& h, State& s, AtomicImage& img) {
            process_light_hits(r, h, s, img);
        },
        [this] (int ray_id, int light_id, ::Ray& ray, State& state) {
            auto& l = scene_.light(light_id);
            // TODO: this pdf depends on the LightTileGen used!
            float pdf_lightpick = 1.0f / scene_.light_count();

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
            state.path_length = 1;

            init_light_mis(ray, state);
        });
}

void DeferredVCM::bounce(State& state, const Intersection& isect, BSDF* bsdf, Ray& ray, bool adjoint, float offset) {
    RNG& rng = state.rng;

    float rr_pdf;
    if (!russian_roulette(state.throughput, rng.random_float(), rr_pdf)) {
        terminate_path(state);
        return;
    }

    float pdf_dir_w;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, rng, BSDF_ALL, sampled_flags, pdf_dir_w);

    bool is_specular = sampled_flags & BSDF_SPECULAR;

    if (sampled_flags == 0 || pdf_dir_w == 0.0f || is_black(bsdf_value)) {
        terminate_path(state);
        return;
    }

    float pdf_rev_w = pdf_dir_w;
    if (!is_specular) // The reverse pdf of specular surfaces is the same as the forward pdf due to symmetry.
        pdf_rev_w = bsdf->pdf(sample_dir, isect.out_dir);

    const float cos_theta_i = adjoint ? fabsf(shading_normal_adjoint(isect.normal, isect.geom_normal, isect.out_dir, sample_dir))
                                      : fabsf(dot(sample_dir, isect.normal));

    if (adjoint)
        update_light_mis(state);
    else
        update_camera_mis(state);

    state.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);
    state.path_length++;

    ray = Ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };
}

void DeferredVCM::process_camera_empties(Ray& r, State& state, AtomicImage& img) {
    // TODO
}

void DeferredVCM::process_camera_hits(Ray& r, Hit& h, State& state, AtomicImage& img) {
    auto& bsdf_mem_arena = bsdf_memory_arenas.local();
    bsdf_mem_arena.free_all();

    RNG& rng = state.rng;
    const auto isect = calculate_intersection(scene_, h, r);
    const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

    if (cos_theta_o == 0.0f) { // Prevent NaNs
        terminate_path(state);
        return;
    }

    auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);

    if (auto emit = isect.mat->emitter()) {
        // A light source was hit directly. Add the weighted contribution.
        float pdf_lightpick = 1.0f / scene_.light_count();
        float pdf_direct_a, pdf_emit_w;

        rgb radiance = emit->radiance(isect.out_dir, isect.geom_normal, pdf_direct_a, pdf_emit_w);

        const float mis_weight = 1.0f; // TODO

        rgb color = state.throughput * radiance * mis_weight;
        add_contribution(img, state.pixel_id, color);

        terminate_path(state);
        return;
    }

    if (state.path_length >= settings_.max_path_len) {
        terminate_path(state);
        return;
    }

    // Continue the path using russian roulette.
    const float offset = h.tmax * 1e-4f;
    bounce(state, isect, bsdf, r, false, offset);
}

void DeferredVCM::process_light_hits(Ray& r, Hit& h, State& state, AtomicImage& img) {
    auto& bsdf_mem_arena = bsdf_memory_arenas.local();
    bsdf_mem_arena.free_all();

    RNG& rng = state.rng;
    const auto isect = calculate_intersection(scene_, h, r);
    const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

    if (cos_theta_o == 0.0f) { // Prevent NaNs
        terminate_path(state);
        return;
    }

    auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);

    if (state.path_length >= settings_.max_path_len) {
        terminate_path(state);
        return;
    }

    // Continue the path using russian roulette.
    const float offset = h.tmax * 1e-4f;
    bounce(state, isect, bsdf, r, false, offset);
}

void DeferredVCM::direct_illum(AtomicImage& out) {

}

void DeferredVCM::connect_to_camera(AtomicImage& out) {

}

void DeferredVCM::connect(AtomicImage& out) {

}

void DeferredVCM::merge(AtomicImage& out) {

}

} // namespace imba
