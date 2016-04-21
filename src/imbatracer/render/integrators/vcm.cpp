#include "vcm.h"
#include "../../core/float4.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <cmath>
#include <chrono>

// Enables debugging code for the vertex cache.
#define ENABLE_VERTEX_CACHE_DEBUG

// Enables debugging code that checks for NaNs in all contributions.
#define ENABLE_NAN_TESTS

namespace imba {

//static int64_t photon_time = 0;
static const float offset = 0.0001f;

using ThreadLocalMemArena = tbb::enumerable_thread_specific<MemoryArena, tbb::cache_aligned_allocator<MemoryArena>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

using ThreadLocalPhotonContainer = tbb::enumerable_thread_specific<std::vector<PhotonIterator>, tbb::cache_aligned_allocator<std::vector<PhotonIterator>>, tbb::ets_key_per_instance>;
static ThreadLocalPhotonContainer photon_containers;

inline float mis_heuristic(float a) {
    //return sqr(a);
    return a;
}

/// Computes the cosine term for adjoint BSDFs that use shading normals.
///
/// This function has to be used for all BSDFs during particle tracing to prevent brighness discontinuities.
/// See Veach's PhD thesis for more details.
inline float shading_normal_adjoint(const float3& normal, const float3& geom_normal, const float3& out_dir, const float3& in_dir) {
    //return dot(in_dir, normal);
    return dot(out_dir, normal) * dot(in_dir, geom_normal) / dot(out_dir, geom_normal);
}

// Reduce ugliness from the template parameters.
#define VCM_TEMPLATE template<bool bpt_only, bool ppm_only, bool lt_only, bool pt_only>
#define VCM_INTEGRATOR VCMIntegrator<bpt_only, ppm_only, lt_only, pt_only>

VCM_TEMPLATE
void VCM_INTEGRATOR::preprocess() {
    if (lt_only || pt_only) // Vertex cache is only needed for bidirectional algorithms.
        return;

    // Trace a couple of light paths into the scene and calculate their average length.
    // This information is than used to allocate the light vertex cache.

    // Setup the queues. We need two: one for input and one for output during scattering.
    // Make sure that the size of the queues follows the proper alignment constraints.
    const int path_count = LIGHT_PATH_LEN_PROBES;

    RayQueue<VCMState>* queues[2] = {
        new RayQueue<VCMState>(path_count),
        new RayQueue<VCMState>(path_count)
    };

    int in_q = 0;
    int out_q = 1;

    // Fill the queue with the initial light rays.
    for (int i = 0; i < path_count; ++i) {
        Ray ray_out;
        VCMState state_out;
        state_out.pixel_id = i;

        static std::random_device rd;
        state_out.rng = RNG(rd());

        // randomly choose one light source to sample
        auto& l = scene_.lights[state_out.rng.random_int(0, scene_.lights.size())];
        float pdf_lightpick = 1.0f / scene_.lights.size();

        Light::EmitSample sample = l->sample_emit(state_out.rng);
        ray_out.org.x = sample.pos.x;
        ray_out.org.y = sample.pos.y;
        ray_out.org.z = sample.pos.z;
        ray_out.org.w = offset;

        ray_out.dir.x = sample.dir.x;
        ray_out.dir.y = sample.dir.y;
        ray_out.dir.z = sample.dir.z;
        ray_out.dir.w = FLT_MAX;

        state_out.throughput = sample.radiance / pdf_lightpick;
        state_out.path_length = 1;
        state_out.continue_prob = 1.0f;

        queues[in_q]->push(ray_out, state_out);
    }

    std::atomic<int> vertex_count(0);

    // Trace the light paths until they are (almost) all terminated.
    while (queues[in_q]->size() > 256) {
        queues[in_q]->traverse(scene_);

        // Process hitpoints and bounce or terminate paths.
        const int ray_count = queues[in_q]->size();
        VCMState* states    = queues[in_q]->states();
        const Hit* hits     = queues[in_q]->hits();
        const Ray* rays     = queues[in_q]->rays();
        auto& rays_out      = *queues[out_q];
        tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count),
                          [this, states, hits, rays, &rays_out, &vertex_count] (const tbb::blocked_range<size_t>& range) {
            auto& bsdf_mem_arena = bsdf_memory_arenas.local();

            for (size_t i = range.begin(); i != range.end(); ++i) {
                float rr_pdf;
                RNG& rng = states[i].rng;
                if (hits[i].tri_id < 0 || !russian_roulette(states[i].throughput, rng.random_float(), rr_pdf))
                    continue;

                bsdf_mem_arena.free_all();

                Intersection isect = calculate_intersection(hits, rays, i);
                auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena, true);

                if (!isect.mat->is_specular())
                    ++vertex_count; // we would store a vertex at this position

                float pdf_dir_w;
                float3 sample_dir;
                BxDFFlags sampled_flags;
                auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, rng, BSDF_ALL, sampled_flags, pdf_dir_w);

                if (sampled_flags == 0 || pdf_dir_w == 0.0f || is_black(bsdf_value))
                    continue;

                const float cos_theta_i = fabsf(shading_normal_adjoint(isect.normal, isect.geom_normal, isect.out_dir, sample_dir));

                VCMState s = states[i];
                s.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);
                s.path_length++;
                s.continue_prob = rr_pdf;

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

    const float avg_len = static_cast<float>(vertex_count) / static_cast<float>(path_count);
    const int vc_size = 1.1f * std::ceil(avg_len) * width_ * height_;

    for (auto& vc : vertex_caches_)
        vc.resize(vc_size);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::add_vertex_to_cache(const LightPathVertex& v, int sample_id) {
    int i = vertex_cache_last_[sample_id]++;
    if (i > vertex_caches_[sample_id].size()) {
#ifdef ENABLE_VERTEX_CACHE_DEBUG
        printf("Attempted to store a vertex that does not fit into the cache.\n");
#endif
        return; // Discard vertices that do not fit. This is statistically very unlikely to happen.
    }

    vertex_caches_[sample_id][i] = v;
}

VCM_TEMPLATE
void VCM_INTEGRATOR::render(AtomicImage& img) {
    reset_buffers();

    cur_iteration_++;

    pm_radius_ = base_radius_ / powf(static_cast<float>(cur_iteration_), 0.5f * (1.0f - radius_alpha_));
    pm_radius_ = std::max(pm_radius_, 1e-7f); // ensure numerical stability

    vm_normalization_ = 1.0f / (sqr(pm_radius_) * pi * light_path_count_);

    // Compute the MIS weights for vetex connection and vertex merging.
    const float etaVCM = pi * sqr(pm_radius_) * light_path_count_;
    mis_weight_vc_ = mis_heuristic(1.0f / etaVCM);
    mis_weight_vm_ = bpt_only ? 0.0f : mis_heuristic(etaVCM);

    if (!pt_only)
        trace_light_paths(img);

    if (!lt_only)
        trace_camera_paths(img);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::reset_buffers() {
    for(auto& i : vertex_cache_last_)
        i = 0;
}

VCM_TEMPLATE
void VCM_INTEGRATOR::trace_light_paths(AtomicImage& img) {
    scheduler_.run_iteration(img, this,
        &VCM_INTEGRATOR::process_shadow_rays,
        &VCM_INTEGRATOR::process_light_rays,
        [this] (int x, int y, ::Ray& ray_out, VCMState& state_out) {
            // randomly choose one light source to sample
            int i = state_out.rng.random_int(0, scene_.lights.size());
            auto& l = scene_.lights[i];
            float pdf_lightpick = 1.0f / scene_.lights.size();

            Light::EmitSample sample = l->sample_emit(state_out.rng);
            ray_out.org.x = sample.pos.x;
            ray_out.org.y = sample.pos.y;
            ray_out.org.z = sample.pos.z;
            ray_out.org.w = offset;

            ray_out.dir.x = sample.dir.x;
            ray_out.dir.y = sample.dir.y;
            ray_out.dir.z = sample.dir.z;
            ray_out.dir.w = FLT_MAX;

            state_out.throughput = sample.radiance / pdf_lightpick;
            state_out.path_length = 1;
            state_out.continue_prob = 1.0f;

            state_out.dVCM = mis_heuristic(sample.pdf_direct_a / sample.pdf_emit_w); // pdf_lightpick cancels out

            if (l->is_delta())
                state_out.dVC = 0.0f;
            else
                state_out.dVC = mis_heuristic(sample.cos_out / (sample.pdf_emit_w * pdf_lightpick));

            state_out.dVM = state_out.dVC * mis_weight_vc_;

            state_out.is_finite = l->is_finite();

#ifdef ENABLE_NAN_TESTS
            if (isnan(state_out.dVC) || isnan(state_out.dVCM) || isnan(state_out.dVM)) {
                printf("NaN in light sampling:\n   pdf_dir_a=%f  pdf_emit_w=%f  cos=%f  pdf_lightpick=%f\n",
                    sample.pdf_direct_a, sample.pdf_emit_w, sample.cos_out, pdf_lightpick);
            }
#endif
        });

    if (lt_only)
        return;

    for (int i = 0; i < spp_; ++i) {
        light_vertices_count_[i] = std::min(static_cast<int>(vertex_caches_[i].size()), static_cast<int>(vertex_cache_last_[i]));
        photon_grid_[i].reserve(light_vertices_count_[i]);
        photon_grid_[i].build(vertex_caches_[i].begin(), vertex_caches_[i].begin() + light_vertices_count_[i], pm_radius_);
    }
}

VCM_TEMPLATE
void VCM_INTEGRATOR::trace_camera_paths(AtomicImage& img) {
    scheduler_.run_iteration(img, this,
        &VCM_INTEGRATOR::process_shadow_rays,
        &VCM_INTEGRATOR::process_camera_rays,
        [this] (int x, int y, ::Ray& ray_out, VCMState& state_out) {
            // Sample a ray from the camera.
            const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
            const float sample_y = static_cast<float>(y) + state_out.rng.random_float();

            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = float4(1.0f);
            state_out.path_length = 1;
            state_out.continue_prob = 1.0f;

            const float3 dir(ray_out.dir.x, ray_out.dir.y, ray_out.dir.z);

            // PDF on image plane is 1. We need to convert this from image plane area to solid angle.
            const float cos_theta_o = dot(dir, cam_.dir());
            assert(cos_theta_o > 0.0f);
            const float pdf_cam_w = sqr(cam_.image_plane_dist() / cos_theta_o) / cos_theta_o;

            state_out.dVC = 0.0f;
            state_out.dVM = 0.0f;
            state_out.dVCM = mis_heuristic(light_path_count_ / pdf_cam_w);
        });
}

VCM_TEMPLATE
void VCM_INTEGRATOR::bounce(VCMState& state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out, bool adjoint) {
    RNG& rng = state.rng;

    float rr_pdf;
    if (!russian_roulette(state.throughput, rng.random_float(), rr_pdf))
        return;

    BxDFFlags flags = BSDF_ALL;
    if (ppm_only && !adjoint) // For PPM: only sample specular scattering on the camera path.
        flags = BxDFFlags(BSDF_SPECULAR | BSDF_REFLECTION | BSDF_TRANSMISSION);

    float pdf_dir_w;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, rng, flags, sampled_flags, pdf_dir_w);

    bool is_specular = sampled_flags & BSDF_SPECULAR;

    if (sampled_flags == 0 || pdf_dir_w == 0.0f || is_black(bsdf_value))
        return;

    float pdf_rev_w = pdf_dir_w;
    if (!is_specular) // cannot evaluate reverse pdf of specular surfaces (but is the same as forward due to symmetry)
        pdf_rev_w = bsdf->pdf(sample_dir, isect.out_dir);

    const float cos_theta_i = adjoint ? fabsf(shading_normal_adjoint(isect.normal, isect.geom_normal, isect.out_dir, sample_dir))
                                      : fabsf(dot(sample_dir, isect.normal));

    VCMState s = state;
    if (is_specular) {
        s.dVCM = 0.0f;
        s.dVC *= mis_heuristic(cos_theta_i);
        s.dVM *= mis_heuristic(cos_theta_i);
    } else {
        s.dVC = mis_heuristic(cos_theta_i / (pdf_dir_w * rr_pdf)) *
                (s.dVC * mis_heuristic(pdf_rev_w * rr_pdf) + s.dVCM + mis_weight_vm_);

        s.dVM = mis_heuristic(cos_theta_i / (pdf_dir_w * rr_pdf)) *
                (s.dVM * mis_heuristic(pdf_rev_w * rr_pdf) + s.dVCM * mis_weight_vc_ + 1.0f);

        s.dVCM = mis_heuristic(1.0f / (pdf_dir_w * rr_pdf));
    }

    s.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);
    s.path_length++;
    s.continue_prob = rr_pdf;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };

    rays_out.push(ray, s);

#ifdef ENABLE_NAN_TESTS
    if (isnan(s.throughput.x) || isnan(s.throughput.y) || isnan(s.throughput.z))
        printf("NaN in bounce: adjoint=%d\n   bsdf=(%f,%f,%f)\n   cos=%f\n   pdf=%f*%f\n",
            adjoint, bsdf_value.x, bsdf_value.y, bsdf_value.z, cos_theta_i, rr_pdf, pdf_dir_w);

    if (isnan(s.dVC) || isnan(s.dVCM)) {
        printf("NaN in bounce - partial weights:\n   cos=%f  pdf=%f  rr_pdf=%f  rev_pdf=%f  vm_weight=%f  vc_weight=%f\n",
            cos_theta_i, pdf_dir_w, rr_pdf, pdf_rev_w, mis_weight_vm_, mis_weight_vc_);
    }
#endif
}

VCM_TEMPLATE
void VCM_INTEGRATOR::process_light_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& rays_out, RayQueue<VCMState>& ray_out_shadow, AtomicImage& img) {
    int ray_count = rays_in.size();
    VCMState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count), [this, states, hits, rays, &rays_out, &ray_out_shadow] (const tbb::blocked_range<size_t>& range) {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0)
                continue;

            if (states[i].path_length >= max_path_len_)
                continue; // Path is too long already. Do not bounce, do not store the vertex.

            bsdf_mem_arena.free_all();

            RNG& rng = states[i].rng;
            Intersection isect = calculate_intersection(hits, rays, i);
            float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            // Complete calculation of the partial weights.
            if (states[i].path_length > 1 || states[i].is_finite)
                states[i].dVCM *= mis_heuristic(sqr(isect.distance));

            states[i].dVCM *= 1.0f / mis_heuristic(cos_theta_o);
            states[i].dVC  *= 1.0f / mis_heuristic(cos_theta_o);
            states[i].dVM  *= 1.0f / mis_heuristic(cos_theta_o);

            auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena, true);

            if (!isect.mat->is_specular()){ // Do not store vertices on materials described by a delta distribution.
                if (!lt_only) {
                    add_vertex_to_cache(LightPathVertex(
                        isect,
                        states[i].throughput,
                        states[i].continue_prob,
                        states[i].dVC,
                        states[i].dVCM,
                        states[i].dVM,
                        states[i].path_length + 1), states[i].sample_id);
                }

                if (!ppm_only)
                    connect_to_camera(states[i], isect, bsdf, ray_out_shadow);
            }

            bounce(states[i], isect, bsdf, rays_out, true);
        }
    });
}

VCM_TEMPLATE
void VCM_INTEGRATOR::connect_to_camera(const VCMState& light_state, const Intersection& isect,
                                      const BSDF* bsdf, RayQueue<VCMState>& ray_out_shadow) {
    float3 dir_to_cam = cam_.pos() - isect.pos;

    if (dot(-dir_to_cam, cam_.dir()) < 0.0f)
        return; // Vertex is behind the camera.

    const float2 raster_pos = cam_.world_to_raster(isect.pos);

    VCMState state = light_state;
    state.pixel_id = cam_.raster_to_id(raster_pos);

    if (state.pixel_id < 0 || state.pixel_id >= width_ * height_)
        return; // The point is outside the image plane.

    // Compute ray direction and distance.
    float dist_to_cam_sqr = lensqr(dir_to_cam);
    float dist_to_cam = sqrt(dist_to_cam_sqr);
    dir_to_cam = dir_to_cam / dist_to_cam;

    const float cos_theta_i = fabsf(dot(cam_.dir(), -dir_to_cam));
    //float cos_theta_o = fabsf(dot(isect.normal, dir_to_cam));
    const float cos_theta_o = fabsf(shading_normal_adjoint(isect.normal, isect.geom_normal, isect.out_dir, dir_to_cam));

    // Evaluate the material and compute the pdf values.
    auto bsdf_value = bsdf->eval(isect.out_dir, dir_to_cam, BSDF_ALL);
    float pdf_rev_w = bsdf->pdf(dir_to_cam, isect.out_dir);

    const float pdf_rev = pdf_rev_w * light_state.continue_prob;

    if (pdf_rev == 0.0f)
        return;

    // Compute conversion factor from surface area to image plane and vice versa.
    const float img_to_surf = (sqr(cam_.image_plane_dist()) * cos_theta_o) /
                              (dist_to_cam_sqr * cos_theta_i * sqr(cos_theta_i));
    const float surf_to_img = 1.0f / img_to_surf;

    // Compute the MIS weight.
    const float pdf_cam = img_to_surf; // Pixel sampling pdf is one as pixel area is one by convention.
    const float mis_weight_light = mis_heuristic(pdf_cam / light_path_count_) * (mis_weight_vm_ + light_state.dVCM + light_state.dVC * mis_heuristic(pdf_rev));

    const float mis_weight = lt_only ? 1.0f : (1.0f / (mis_weight_light + 1.0f));

    // Contribution is divided by the number of samples (light_path_count_) and the factor that converts the (divided) pdf from surface area to image plane area.
    // The cosine term is already included in the img_to_surf term.
    state.throughput *= mis_weight * bsdf_value * img_to_surf / light_path_count_;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { dir_to_cam.x, dir_to_cam.y, dir_to_cam.z, dist_to_cam - offset }
    };

    ray_out_shadow.push(ray, state);

#ifdef ENABLE_NAN_TESTS
    if (isnan(state.throughput.x) || isnan(state.throughput.y) || isnan(state.throughput.z)) {
        printf("NaN in connect to camera:\n   mis=%f\n   bsdf=(%f,%f,%f)\n   weight=%f/%f\n",
            mis_weight, bsdf_value.x, bsdf_value.y, bsdf_value.z, img_to_surf, light_path_count_);
        printf("   mis_weight = %f * (%f + %f + %f * %f)\n", mis_heuristic(pdf_cam / light_path_count_), mis_weight_vm_, light_state.dVCM, light_state.dVC, mis_heuristic(pdf_rev));
        printf("   bounces=%d\n", state.path_length);
    }
#endif
}

VCM_TEMPLATE
void VCM_INTEGRATOR::process_camera_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& rays_out, RayQueue<VCMState>& ray_out_shadow, AtomicImage& img) {
    int ray_count = rays_in.size();
    VCMState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count), [this, states, hits, rays, &rays_out, &ray_out_shadow, &img] (const tbb::blocked_range<size_t>& range) {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (hits[i].tri_id < 0)
                continue;

            bsdf_mem_arena.free_all();

            RNG& rng = states[i].rng;
            Intersection isect = calculate_intersection(hits, rays, i);
            float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);

            if (ppm_only){
                if (!isect.mat->is_specular()) {
                    vertex_merging(states[i], isect, bsdf, img);
                }

                // Continue the path using russian roulette.
                if (states[i].path_length < max_path_len_)
                    bounce(states[i], isect, bsdf, rays_out, false);

                continue;
            }

            // Complete computation of partial MIS weights.
            states[i].dVCM *= mis_heuristic(sqr(isect.distance)) / mis_heuristic(cos_theta_o); // convert divided pdf from solid angle to area
            states[i].dVC *= 1.0f / mis_heuristic(cos_theta_o);
            states[i].dVM *= 1.0f / mis_heuristic(cos_theta_o);

            if (isect.mat->light()) {
                auto light_source = isect.mat->light();

                // A light source was hit directly. Add the weighted contribution.
                float pdf_lightpick = 1.0f / scene_.lights.size();
                float pdf_direct_a, pdf_emit_w;
                float4 radiance = light_source->radiance(isect.out_dir, pdf_direct_a, pdf_emit_w);

                const float pdf_di = pdf_direct_a * pdf_lightpick;
                const float pdf_e = pdf_emit_w * pdf_lightpick;

                const float mis_weight_camera = mis_heuristic(pdf_di) * states[i].dVCM + mis_heuristic(pdf_e) * states[i].dVC;
                const float mis_weight = 1.0f / (mis_weight_camera + 1.0f);

                if (states[i].path_length > 1) {
                    float4 color = states[i].throughput * radiance * mis_weight;

                    if (!pt_only)
                        img.pixels()[states[i].pixel_id] += color;
                } else
                    img.pixels()[states[i].pixel_id] += radiance; // Light directly visible, no weighting required.
            }

            // Compute direct illumination.
            if (states[i].path_length < max_path_len_)
                direct_illum(states[i], isect, bsdf, ray_out_shadow);
            else
                continue; // No point in continuing this path. It is too long already

            // Connect to light path vertices.
            if (!pt_only && !isect.mat->is_specular())
                connect(states[i], isect, bsdf, bsdf_mem_arena, ray_out_shadow);

            if (!bpt_only) {
                if (!isect.mat->is_specular())
                    vertex_merging(states[i], isect, bsdf, img);
            }

            // Continue the path using russian roulette.
            bounce(states[i], isect, bsdf, rays_out, false);
        }
    });
}

VCM_TEMPLATE
void VCM_INTEGRATOR::direct_illum(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out_shadow) {
    RNG& rng = cam_state.rng;

    // Generate the shadow ray (sample one point on one lightsource)
    const int light_i = rng.random_int(0, scene_.lights.size());
    const auto ls = scene_.lights[light_i].get();
    const float inv_pdf_lightpick = scene_.lights.size();
    const auto sample = ls->sample_direct(isect.pos, rng);
    const float cos_theta_o = sample.cos_out;
    assert_normalized(sample.dir);

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample.dir.x, sample.dir.y, sample.dir.z, sample.distance - offset }
    };

    // Evaluate the bsdf.
    const float cos_theta_i = fabsf(dot(isect.normal, sample.dir));
    auto bsdf_value = bsdf->eval(isect.out_dir, sample.dir, BSDF_ALL);
    float pdf_dir_w = bsdf->pdf(isect.out_dir, sample.dir);
    float pdf_rev_w = bsdf->pdf(sample.dir, isect.out_dir);

    if (pdf_dir_w == 0.0f || pdf_rev_w == 0.0f)
        return;

    const float pdf_forward = ls->is_delta() ? 0.0f : cam_state.continue_prob * pdf_dir_w;
    const float pdf_reverse = cam_state.continue_prob * pdf_rev_w;

    // Compute full MIS weights for camera and light.
    const float mis_weight_light = mis_heuristic(pdf_forward * inv_pdf_lightpick / sample.pdf_direct_w);
    const float mis_weight_camera = mis_heuristic(sample.pdf_emit_w * cos_theta_i / (sample.pdf_direct_w * cos_theta_o)) *
                                    (mis_weight_vm_ + cam_state.dVCM + cam_state.dVC * mis_heuristic(pdf_reverse));

    const float mis_weight = pt_only ? 1.0f : (1.0f / (mis_weight_camera + 1.0f + mis_weight_light));

    VCMState s = cam_state;
    s.throughput *= mis_weight * bsdf_value * cos_theta_i * sample.radiance * inv_pdf_lightpick;

    rays_out_shadow.push(ray, s);

#ifdef ENABLE_NAN_TESTS
    if (isnan(s.throughput.x) || isnan(s.throughput.y) || isnan(s.throughput.z))
        printf("NaN in direct illumination:\n   mis=%f\n   bsdf=(%f,%f,%f)\n   cos=%f\n   pdf=%f\n   radiance=(%f,%f,%f)\n",
            mis_weight, bsdf_value.x, bsdf_value.y, bsdf_value.z, cos_theta_i, inv_pdf_lightpick,
            sample.radiance.x, sample.radiance.y, sample.radiance.z);
#endif
}

VCM_TEMPLATE
void VCM_INTEGRATOR::connect(VCMState& cam_state, const Intersection& isect, BSDF* bsdf_cam, MemoryArena& bsdf_arena, RayQueue<VCMState>& rays_out_shadow) {
    // Additional weight required do to changed random processes when using the vertex cache.
    const float vc_weight = (light_vertices_count_[cam_state.sample_id] / light_path_count_) / NUM_CONNECTIONS;

    RNG& rng = cam_state.rng;
    // Connect to NUM_CONNECTIONS randomly chosen vertices from the cache.
    for (int i = 0; i < NUM_CONNECTIONS; ++i) {
        auto& light_vertex = vertex_caches_[cam_state.sample_id][rng.random_int(0, light_vertices_count_[cam_state.sample_id])];

        // Ignore paths that are longer than the specified maximum length.
        if (light_vertex.path_length + cam_state.path_length > max_path_len_)
            continue;

        auto light_bsdf = light_vertex.isect.mat->get_bsdf(light_vertex.isect, bsdf_arena, true);

        // Compute connection direction and distance.
        float3 connect_dir = light_vertex.isect.pos - isect.pos;
        const float connect_dist_sq = lensqr(connect_dir);
        const float connect_dist = std::sqrt(connect_dist_sq);
        connect_dir *= 1.0f / connect_dist;

        if (connect_dist < base_radius_) {
            // If two points are too close to each other, they are either occluded or have cosine terms
            // that are close to zero. Numerical inaccuracies might yield an overly bright pixel.
            // The correct result is usually black or close to black so we just ignore those connections.
            continue;
        }

        // Evaluate the bsdf at the camera vertex.
        auto bsdf_value_cam = bsdf_cam->eval(isect.out_dir, connect_dir, BSDF_ALL);
        float pdf_dir_cam_w = bsdf_cam->pdf(isect.out_dir, connect_dir);
        float pdf_rev_cam_w = bsdf_cam->pdf(connect_dir, isect.out_dir);

        // Evaluate the bsdf at the light vertex.
        auto bsdf_value_light = light_bsdf->eval(light_vertex.isect.out_dir, -connect_dir, BSDF_ALL);
        float pdf_dir_light_w = light_bsdf->pdf(light_vertex.isect.out_dir, -connect_dir);
        float pdf_rev_light_w = light_bsdf->pdf(-connect_dir, light_vertex.isect.out_dir);

        if (pdf_dir_cam_w == 0.0f || pdf_dir_light_w == 0.0f ||
            pdf_rev_cam_w == 0.0f || pdf_rev_light_w == 0.0f)
            continue;  // A pdf value of zero means that there has to be zero contribution from this pair of directions as well.

        // Compute the cosine terms. We need to use the adjoint for the light vertex BSDF.
        const float cos_theta_cam = dot(isect.normal, connect_dir);
        const float cos_theta_light = shading_normal_adjoint(light_vertex.isect.normal, light_vertex.isect.geom_normal,
                                                             light_vertex.isect.out_dir, -connect_dir);

        const float geom_term = cos_theta_cam * cos_theta_light / connect_dist_sq;
        if (geom_term <= 0.0f)
            continue;

        // Compute and convert the pdfs
        const float pdf_cam_f = pdf_dir_cam_w * cam_state.continue_prob;
        const float pdf_cam_r = pdf_rev_cam_w * cam_state.continue_prob;

        const float pdf_light_f = pdf_dir_light_w * light_vertex.continue_prob;
        const float pdf_light_r = pdf_rev_light_w * light_vertex.continue_prob;

        const float pdf_cam_a = pdf_cam_f * cos_theta_light / connect_dist_sq;
        const float pdf_light_a = pdf_light_f * cos_theta_cam / connect_dist_sq;

        // Compute the full MIS weight from the partial weights and pdfs.
        const float mis_weight_light = mis_heuristic(pdf_cam_a) * (mis_weight_vm_ + light_vertex.dVCM + light_vertex.dVC * mis_heuristic(pdf_light_r));
        const float mis_weight_camera = mis_heuristic(pdf_light_a) * (mis_weight_vm_ + cam_state.dVCM + cam_state.dVC * mis_heuristic(pdf_cam_r));

        const float mis_weight = 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);

        VCMState s = cam_state;
        s.throughput *= vc_weight * mis_weight * geom_term * bsdf_value_cam * bsdf_value_light * light_vertex.throughput;

        Ray ray {
            { isect.pos.x, isect.pos.y, isect.pos.z, offset },
            { connect_dir.x, connect_dir.y, connect_dir.z, connect_dist - offset }
        };

        rays_out_shadow.push(ray, s);

#ifdef ENABLE_NAN_TESTS
        if (isnan(s.throughput.x) || isnan(s.throughput.y) || isnan(s.throughput.z))
            printf("NaN in vertex connection:\n   vc_weight=%f\n   mis=%f\n   geom_term=%f\n   bsdf_cam=(%f,%f,%f)\n   bsdf_light=(%f,%f,%f)\n   throughput=(%f,%f,%f)\n",
                vc_weight, mis_weight, geom_term, bsdf_value_cam.x, bsdf_value_cam.y, bsdf_value_cam.z,
                bsdf_value_light.x, bsdf_value_light.y, bsdf_value_light.z,
                light_vertex.throughput.x, light_vertex.throughput.y, light_vertex.throughput.z);
#endif
    }
}

VCM_TEMPLATE
void VCM_INTEGRATOR::vertex_merging(const VCMState& state, const Intersection& isect, const BSDF* bsdf, AtomicImage& img) {
    auto& photons = photon_containers.local();
    photons.reserve(width_ * height_);
    photons.clear();

    //auto t0 = std::chrono::high_resolution_clock::now();
    photon_grid_[state.sample_id].process(photons, isect.pos);
    //auto t1 = std::chrono::high_resolution_clock::now();
    //photon_time += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    float4 contrib(0.0f);
    for (PhotonIterator p : photons) {
        // Ignore the path if it would be too small. Don't count the merged vertices twice.
        if (state.path_length + p->path_length - 1 > max_path_len_)
            continue;

        const auto light_in_dir = p->isect.out_dir;

        const auto bsdf_value = bsdf->eval(isect.out_dir, light_in_dir);
        const float pdf_dir_w = bsdf->pdf(isect.out_dir, light_in_dir);
        const float pdf_rev_w = bsdf->pdf(light_in_dir, isect.out_dir);

        if (is_black(bsdf_value))
            continue;

        const float pdf_forward = pdf_dir_w * state.continue_prob;
        const float pdf_reverse = pdf_rev_w * state.continue_prob;

        const float mis_weight_light = p->dVCM * mis_weight_vc_ + p->dVM * mis_heuristic(pdf_forward);
        const float mis_weight_camera = state.dVCM * mis_weight_vc_ + state.dVM * mis_heuristic(pdf_reverse);

        const float mis_weight = ppm_only ? 1.0f : (1.0f / (mis_weight_light + 1.0f + mis_weight_camera));

        contrib += mis_weight * bsdf_value * p->throughput;
    }

    img.pixels()[state.pixel_id] += state.throughput * contrib * vm_normalization_;
}

VCM_TEMPLATE
void VCM_INTEGRATOR::process_shadow_rays(RayQueue<VCMState>& rays_in, AtomicImage& img) {
    int ray_count = rays_in.size();
    const VCMState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    const Ray* rays = rays_in.rays();

    tbb::parallel_for(tbb::blocked_range<size_t>(0, ray_count), [states, hits, &img] (const tbb::blocked_range<size_t>& range) {
        // const float4 max_contrib(100.0f, 100.0f, 100.0f, 100.0f);
        // const float4 min_contrib(0.0f, 0.0f, 0.0f, 0.0f);

        for (size_t i = range.begin(); i != range.end(); ++i)
            if (hits[i].tri_id < 0)
                img.pixels()[states[i].pixel_id] += /*clamp(*/states[i].throughput;//, min_contrib, max_contrib);
    });
}

// Prevents linker errors from defining member functions of a template class outside of the header.
void dummy_func_to_prevent_linker_errors_dont_call() {
    Scene scene;
    PerspectiveCamera cam(0,0,0.0f);
    PixelRayGen<VCMState> ray_gen(1, 1, 1);
    VCMIntegrator<false, false, false, false> tmp1(scene, cam, ray_gen, 1, 1, 1, 1);
    VCMIntegrator<true , false, false, false> tmp2(scene, cam, ray_gen, 1, 1, 1, 1);
    VCMIntegrator<false, true , false, false> tmp3(scene, cam, ray_gen, 1, 1, 1, 1);
    VCMIntegrator<false, false, true , false> tmp4(scene, cam, ray_gen, 1, 1, 1, 1);
    VCMIntegrator<false, false, false, true > tmp5(scene, cam, ray_gen, 1, 1, 1, 1);
}

} // namespace imba

