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
    PartialMIS::setup_iteration(pm_radius_, settings_.light_path_count, MIS_NEXTEVT_CAM);

    cam_verts_->clear();
    light_verts_->clear();

    trace_camera_paths();
    //trace_light_paths();

    path_tracing(img);
    light_tracing(img);
    connect(img);
    merge(img);
}

void DeferredVCM::trace_camera_paths() {
    // TODO: refactor the AtomicImage out of here (by removing it / adding a special case in the scheduler)

    DeferredScheduler<State>::ShadeEmptyFn env_hit;
    if (scene_.env_map()) {
        env_hit = [this] (Ray& r, State& s) {
            process_envmap_hits(r, s);
        };
    }

    scheduler_.run_iteration(&camera_tile_gen_,
        env_hit,
        [this] (Ray& r, Hit& h, State& s) {
            process_hits(r, h, s, cam_verts_.get());
        },
        [this] (int x, int y, Ray& ray, State& state) {
            // Sample a ray from the camera.
            const float sample_x = static_cast<float>(x) + state.rng.random_float();
            const float sample_y = static_cast<float>(y) + state.rng.random_float();

            ray = cam_.generate_ray(sample_x, sample_y);

            state.throughput = rgb(1.0f);
            state.path_length = 1;

            float pdf = cam_.pdf(ray.dir);

            state.mis.init_camera(pdf);
        });
}

void DeferredVCM::trace_light_paths() {
    scheduler_.run_iteration(&light_tile_gen_,
        nullptr,
        [this] (Ray& r, Hit& h, State& s) {
            process_hits(r, h, s, light_verts_.get());
        },
        [this] (int ray_id, int light_id, ::Ray& ray, State& state) {
            auto& l = scene_.light(light_id);
            // TODO: this pdf depends on the LightTileGen used!
            float pdf_lightpick = 1.0f / scene_.light_count();

            Light::EmitSample sample = l->sample_emit(state.rng);
            ray.org = make_vec4(sample.pos, 1e-4f);
            ray.dir = make_vec4(sample.dir, FLT_MAX);

            state.throughput = sample.radiance / pdf_lightpick;
            state.path_length = 1;

            state.mis.init_light(sample.pdf_emit_w, sample.pdf_direct_a, pdf_lightpick, sample.cos_out, l->is_finite(), l->is_delta());

            light_verts_->add(Vertex(state.mis, state.throughput, -1, light_id, 1, sample.pos));
        });
}

void DeferredVCM::bounce(State& state, const Intersection& isect, BSDF* bsdf, Ray& ray, bool adjoint, float offset) {
    if (state.path_length >= settings_.max_path_len) {
        terminate_path(state);
        return;
    }

    float rr_pdf;
    if (!russian_roulette(state.throughput, state.rng.random_float(), rr_pdf)) {
        terminate_path(state);
        return;
    }

    float pdf_dir_w;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, state.rng, BSDF_ALL, sampled_flags, pdf_dir_w);

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

    state.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);
    state.path_length++;
    state.mis.update_bounce(pdf_dir_w, pdf_rev_w, cos_theta_i, is_specular);

    ray = Ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };
}

void DeferredVCM::process_envmap_hits(Ray& r, State& state) {
    // TODO
    // The environment map was "hit"
    // I should record this hit somehow, to
    //   a) help guiding environment map sampling
    //   b) do the environment map hit evaluation in a deferred way as well, consistent with the rest of the algorithm
}

void DeferredVCM::process_hits(Ray& r, Hit& h, State& state, VertCache* cache) {
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

    state.mis.update_hit(cos_theta_o, h.tmax * h.tmax);

    if (!isect.mat->is_specular())
        state.ancestor = cache->add(Vertex(state.mis, state.throughput, state.ancestor, state.pixel_id, state.path_length, isect));
    else
        state.ancestor = -1;

    // Continue the path using russian roulette.
    const float offset = h.tmax * 1e-4f;
    bounce(state, isect, bsdf, r, false, offset);
}

void DeferredVCM::path_tracing(AtomicImage& out) {
    ArrayTileGen<ShadowState> tile_gen(settings_.tile_size, cam_verts_->size());
    shadow_scheduler_.run_iteration(&tile_gen,
        [this, &out] (Ray& r, ShadowState& s) {
            add_contribution(out, s.pixel_id, s.contrib);
        },
        nullptr, // hits --> occluded
        [this] (int vert_id, int unused, ::Ray& ray, ShadowState& state) {
            const auto& cam_v   = *cam_verts_;
            const auto& light_v = *light_verts_;
            const auto& v = cam_v[vert_id];

            // TODO: special case if the hit is on a light source

            // Sample a point on a light
            const auto& ls = scene_.light(state.rng.random_int(0, scene_.light_count()));
            const float pdf_lightpick_inv = scene_.light_count();
            const auto sample = ls->sample_direct(v.isect.pos, state.rng);
            const float cos_theta_i = fabsf(dot(v.isect.normal, sample.dir));

            // Evaluate the BSDF and compute the pdf values
            auto& bsdf_mem_arena = bsdf_memory_arenas.local();
            bsdf_mem_arena.free_all();
            auto bsdf = v.isect.mat->get_bsdf(v.isect, bsdf_mem_arena);

            auto bsdf_value = bsdf->eval(v.isect.out_dir, sample.dir, BSDF_ALL);
            float pdf_dir_w = bsdf->pdf(v.isect.out_dir, sample.dir);
            float pdf_rev_w = bsdf->pdf(sample.dir, v.isect.out_dir);

            if (pdf_dir_w == 0.0f || pdf_rev_w == 0.0f)
                return;

            const float mis_weight = mis_weight_di(v.mis, pdf_dir_w, pdf_rev_w,
                                                   sample.pdf_direct_w, sample.pdf_emit_w, pdf_lightpick_inv,
                                                   cos_theta_i, sample.cos_out, ls->is_delta());

            const float offset = 1e-3f * (sample.distance == FLT_MAX ? 1.0f : sample.distance);

            ray.org = make_vec4(v.isect.pos, offset);
            ray.dir = make_vec4(sample.dir, sample.distance - offset);

            state.contrib  = v.throughput * bsdf_value * cos_theta_i * sample.radiance * mis_weight * pdf_lightpick_inv;
            state.pixel_id = v.pixel_id;
        });
}

void DeferredVCM::light_tracing(AtomicImage& out) {

}

void DeferredVCM::connect(AtomicImage& out) {

}

void DeferredVCM::merge(AtomicImage& out) {

}

} // namespace imba
