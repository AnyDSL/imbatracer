#include "vcm.h"
#include "../../core/rgb.h"
#include "../../core/common.h"
#include "../random.h"

#include <cfloat>
#include <cassert>
#include <cmath>
#include <chrono>

namespace imba {

// Offset to prevent self intersection.
static const float offset = 0.0001f;

// Thread-local storage for BSDF objects.
using ThreadLocalMemArena = tbb::enumerable_thread_specific<MemoryArena, tbb::cache_aligned_allocator<MemoryArena>, tbb::ets_key_per_instance>;
static ThreadLocalMemArena bsdf_memory_arenas;

// Thread-local storage for the results of a photon query.
using ThreadLocalPhotonContainer = tbb::enumerable_thread_specific<std::vector<const VCMPhoton*>, tbb::cache_aligned_allocator<std::vector<const VCMPhoton*>>, tbb::ets_key_per_instance>;
static ThreadLocalPhotonContainer photon_containers;

// Reduce ugliness from the template parameters.
#define VCM_TEMPLATE template <VCMSubAlgorithm algo>
#define VCM_INTEGRATOR VCMIntegrator<algo>

VCM_TEMPLATE
void VCM_INTEGRATOR::render(AtomicImage& img) {
    light_vertices_.clear();

    // Shrink the photon mapping radius for the next iteration. Every frame is an iteration of Progressive Photon Mapping.
    cur_iteration_++;
    pm_radius_ = base_radius_ / powf(static_cast<float>(cur_iteration_), 0.5f * (1.0f - radius_alpha_));
    pm_radius_ = std::max(pm_radius_, 1e-7f); // ensure numerical stability
    vm_normalization_ = 1.0f / (sqr(pm_radius_) * pi * light_path_count_);

    // Compute the partial MIS weights for vetex connection and vertex merging.
    // See technical report "Implementing Vertex Connection and Merging".
    const float eta_vcm = pi * sqr(pm_radius_) * light_path_count_;
    mis_eta_vc_ = mis_pow(1.0f / eta_vcm);
    mis_eta_vm_ = algo == ALGO_BPT ? 0.0f : mis_pow(eta_vcm);

    if (algo != ALGO_PT)
        trace_light_paths(img);

    if (algo != ALGO_LT)
        trace_camera_paths(img);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::trace_light_paths(AtomicImage& img) {
    scheduler_.run_iteration(img,
        [this] (RayQueue<VCMState>& ray_in, AtomicImage& out) { process_shadow_rays(ray_in, out); },
        [this] (RayQueue<VCMState>& ray_in, RayQueue<VCMState>& ray_out_shadow, AtomicImage& out) {
            process_light_rays(ray_in, ray_out_shadow, out);
        },
        [this] (int x, int y, ::Ray& ray_out, VCMState& state_out) {
            // randomly choose one light source to sample
            int i = state_out.rng.random_int(0, scene_.light_count());
            auto& l = scene_.light(i);
            float pdf_lightpick = 1.0f / scene_.light_count();

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

            state_out.dVCM = mis_pow(sample.pdf_direct_a / sample.pdf_emit_w); // pdf_lightpick cancels out

            if (l->is_delta())
                state_out.dVC = 0.0f;
            else
                state_out.dVC = mis_pow(sample.cos_out / (sample.pdf_emit_w * pdf_lightpick));

            state_out.dVM = state_out.dVC * mis_eta_vc_;

            state_out.finite_light = l->is_finite();
        });

    if (algo != ALGO_LT) // Only build the hash grid when it is used.
        light_vertices_.build(pm_radius_, algo != ALGO_BPT);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::trace_camera_paths(AtomicImage& img) {
    scheduler_.run_iteration(img,
        [this] (RayQueue<VCMState>& ray_in, AtomicImage& out) { process_shadow_rays(ray_in, out); },
        [this] (RayQueue<VCMState>& ray_in, RayQueue<VCMState>& ray_out_shadow, AtomicImage& out) {
            process_camera_rays(ray_in, ray_out_shadow, out);
        },
        [this] (int x, int y, ::Ray& ray_out, VCMState& state_out) {
            // Sample a ray from the camera.
            const float sample_x = static_cast<float>(x) + state_out.rng.random_float();
            const float sample_y = static_cast<float>(y) + state_out.rng.random_float();

            ray_out = cam_.generate_ray(sample_x, sample_y);

            state_out.throughput = rgb(1.0f);
            state_out.path_length = 1;

            const float3 dir(ray_out.dir.x, ray_out.dir.y, ray_out.dir.z);

            // PDF on image plane is 1. We need to convert this from image plane area to solid angle.
            const float cos_theta_o = dot(dir, cam_.dir());
            assert(cos_theta_o > 0.0f);
            const float pdf_cam_w = sqr(cam_.image_plane_dist() / cos_theta_o) / cos_theta_o;

            state_out.dVC = 0.0f;
            state_out.dVM = 0.0f;
            state_out.dVCM = mis_pow(light_path_count_ / pdf_cam_w);
        });
}

VCM_TEMPLATE
void VCM_INTEGRATOR::bounce(VCMState& state_out, const Intersection& isect, BSDF* bsdf, Ray& ray_out, bool adjoint) {
    RNG& rng = state_out.rng;

    float rr_pdf;
    if (!russian_roulette(state_out.throughput, rng.random_float(), rr_pdf)) {
        terminate_path(state_out);
        return;
    }

    BxDFFlags flags = BSDF_ALL;
    if (algo == ALGO_PPM && !adjoint) // For PPM: only sample specular scattering on the camera path.
        flags = BxDFFlags(BSDF_SPECULAR | BSDF_REFLECTION | BSDF_TRANSMISSION);

    float pdf_dir_w;
    float3 sample_dir;
    BxDFFlags sampled_flags;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, rng, flags, sampled_flags, pdf_dir_w);

    bool is_specular = sampled_flags & BSDF_SPECULAR;

    if (sampled_flags == 0 || pdf_dir_w == 0.0f || is_black(bsdf_value)) {
        terminate_path(state_out);
        return;
    }

    float pdf_rev_w = pdf_dir_w;
    if (!is_specular) // cannot evaluate reverse pdf of specular surfaces (but is the same as forward due to symmetry)
        pdf_rev_w = bsdf->pdf(sample_dir, isect.out_dir);

    const float cos_theta_i = adjoint ? fabsf(shading_normal_adjoint(isect.normal, isect.geom_normal, isect.out_dir, sample_dir))
                                      : fabsf(dot(sample_dir, isect.normal));

    if (is_specular) {
        state_out.dVCM = 0.0f;
        state_out.dVC *= mis_pow(cos_theta_i);
        state_out.dVM *= mis_pow(cos_theta_i);
    } else {
        state_out.dVC = mis_pow(cos_theta_i / pdf_dir_w) *
                (state_out.dVC * mis_pow(pdf_rev_w) + state_out.dVCM + mis_eta_vm_);

        state_out.dVM = mis_pow(cos_theta_i / pdf_dir_w) *
                (state_out.dVM * mis_pow(pdf_rev_w) + state_out.dVCM * mis_eta_vc_ + 1.0f);

        state_out.dVCM = mis_pow(1.0f / pdf_dir_w);
    }

    state_out.throughput *= bsdf_value * cos_theta_i / (rr_pdf * pdf_dir_w);
    state_out.path_length++;

    ray_out = Ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };
}

VCM_TEMPLATE
void VCM_INTEGRATOR::process_light_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& ray_out_shadow, AtomicImage& img) {
    VCMState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    Ray* rays = rays_in.rays();

    compact_hits(scene_, rays_in);

    tbb::parallel_for(tbb::blocked_range<int>(0, rays_in.size()), [&] (const tbb::blocked_range<int>& range) {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (auto i = range.begin(); i != range.end(); ++i) {
            bsdf_mem_arena.free_all();

            RNG& rng = states[i].rng;
            const auto isect = calculate_intersection(scene_, hits[i], rays[i]);
            const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            if (cos_theta_o == 0.0f) { // Prevent NaNs
                terminate_path(states[i]);
                continue;
            }

            // Complete calculation of the partial weights.
            if (states[i].path_length > 1 || states[i].finite_light)
                states[i].dVCM *= mis_pow(sqr(isect.distance));

            states[i].dVCM *= 1.0f / mis_pow(cos_theta_o);
            states[i].dVC  *= 1.0f / mis_pow(cos_theta_o);
            states[i].dVM  *= 1.0f / mis_pow(cos_theta_o);

            auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena, true);

            if (!isect.mat->is_specular()){ // Do not store vertices on materials described by a delta distribution.
                if (algo != ALGO_LT) {
                    light_vertices_.add_vertex_to_cache(LightPathVertex(
                        isect,
                        states[i].throughput,
                        states[i].dVC,
                        states[i].dVCM,
                        states[i].dVM,
                        states[i].path_length + 1), states[i].sample_id);
                }

                if (algo != ALGO_PPM)
                    connect_to_camera(states[i], isect, bsdf, ray_out_shadow);
            }

            bounce(states[i], isect, bsdf, rays[i], true);
        }
    });

    compact_rays(scene_, rays_in);
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

    const float cos_theta_cam = fabsf(dot(cam_.dir(), -dir_to_cam));
    const float cos_theta_surf = fabsf(shading_normal_adjoint(isect.normal, isect.geom_normal, isect.out_dir, dir_to_cam));

    // Evaluate the material and compute the pdf values.
    auto bsdf_value = bsdf->eval(isect.out_dir, dir_to_cam, BSDF_ALL);
    float pdf_rev_w = bsdf->pdf(dir_to_cam, isect.out_dir);

    if (pdf_rev_w == 0.0f)
        return;

    // Compute conversion factor from surface area to image plane and vice versa.
    const float img_to_surf = (sqr(cam_.image_plane_dist()) * cos_theta_surf) /
                              (dist_to_cam_sqr * cos_theta_cam * sqr(cos_theta_cam));
    const float surf_to_img = 1.0f / img_to_surf;

    // Compute the MIS weight.
    const float pdf_cam = img_to_surf; // Pixel sampling pdf is one as pixel area is one by convention.
    const float mis_weight_light = mis_pow(pdf_cam / light_path_count_) * (mis_eta_vm_ + light_state.dVCM + light_state.dVC * mis_pow(pdf_rev_w));

    const float mis_weight = algo == ALGO_LT ? 1.0f : (1.0f / (mis_weight_light + 1.0f));

    // Contribution is divided by the number of samples (light_path_count_) and the factor that converts the (divided) pdf from surface area to image plane area.
    // The cosine term is already included in the img_to_surf term.
    state.throughput *= mis_weight * bsdf_value * img_to_surf / light_path_count_;

    Ray ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { dir_to_cam.x, dir_to_cam.y, dir_to_cam.z, dist_to_cam - offset }
    };

    ray_out_shadow.push(ray, state);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::process_camera_rays(RayQueue<VCMState>& rays_in, RayQueue<VCMState>& ray_out_shadow, AtomicImage& img) {
    VCMState* states = rays_in.states();
    const Hit* hits = rays_in.hits();
    Ray* rays = rays_in.rays();

    compact_hits(scene_, rays_in);

    tbb::parallel_for(tbb::blocked_range<int>(0, rays_in.size()), [&] (const tbb::blocked_range<int>& range) {
        auto& bsdf_mem_arena = bsdf_memory_arenas.local();

        for (auto i = range.begin(); i != range.end(); ++i) {
            bsdf_mem_arena.free_all();

            RNG& rng = states[i].rng;
            const auto isect = calculate_intersection(scene_, hits[i], rays[i]);
            const float cos_theta_o = fabsf(dot(isect.out_dir, isect.normal));

            auto bsdf = isect.mat->get_bsdf(isect, bsdf_mem_arena);

            // Complete computation of partial MIS weights.
            states[i].dVCM *= mis_pow(sqr(isect.distance)) / mis_pow(cos_theta_o); // convert divided pdf from solid angle to area
            states[i].dVC *= 1.0f / mis_pow(cos_theta_o);
            states[i].dVM *= 1.0f / mis_pow(cos_theta_o);

            if (cos_theta_o == 0.0f) { // Prevent NaNs
                terminate_path(states[i]);
                continue;
            }

            if (auto emit = isect.mat->emitter()) {
                // A light source was hit directly. Add the weighted contribution.
                float pdf_lightpick = 1.0f / scene_.light_count();
                float pdf_direct_a, pdf_emit_w;

                rgb radiance = emit->radiance(isect.out_dir, isect.geom_normal, pdf_direct_a, pdf_emit_w);

                const float pdf_di = pdf_direct_a * pdf_lightpick;
                const float pdf_e = pdf_emit_w * pdf_lightpick;

                const float mis_weight_camera = mis_pow(pdf_di) * states[i].dVCM + mis_pow(pdf_e) * states[i].dVC;
                const float mis_weight = 1.0f / (mis_weight_camera + 1.0f);

                if (states[i].path_length > 1) {
                    rgb color = states[i].throughput * radiance * (algo == ALGO_PPM ? 1.0f : mis_weight);

                    if (algo != ALGO_PT)
                        add_contribution(img, states[i].pixel_id, color);
                } else
                    add_contribution(img, states[i].pixel_id, radiance); // Light directly visible, no weighting required.

                terminate_path(states[i]);
                continue;
            }

            // Compute direct illumination.
            if (states[i].path_length < max_path_len_) {
                if (algo != ALGO_PPM)
                    direct_illum(states[i], isect, bsdf, ray_out_shadow);
            } else {
                terminate_path(states[i]);
                continue; // No point in continuing this path. It is too long already
            }

            // Connect to light path vertices.
            if (algo != ALGO_PT && algo != ALGO_PPM && !isect.mat->is_specular())
                connect(states[i], isect, bsdf, bsdf_mem_arena, ray_out_shadow);

            if (algo != ALGO_BPT) {
                if (!isect.mat->is_specular())
                    vertex_merging(states[i], isect, bsdf, img);
            }

            // Continue the path using russian roulette.
            bounce(states[i], isect, bsdf, rays[i], false);
        }
    });

    compact_rays(scene_, rays_in);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::direct_illum(VCMState& cam_state, const Intersection& isect, BSDF* bsdf, RayQueue<VCMState>& rays_out_shadow) {
    // Generate the shadow ray (sample one point on one lightsource)
    const auto& ls = scene_.light(cam_state.rng.random_int(0, scene_.light_count()));
    const float pdf_lightpick_inv = scene_.light_count();
    const auto sample = ls->sample_direct(isect.pos, cam_state.rng);
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

    const float pdf_forward = ls->is_delta() ? 0.0f : pdf_dir_w;

    // Compute full MIS weights for camera and light.
    const float mis_weight_light = mis_pow(pdf_forward * pdf_lightpick_inv / sample.pdf_direct_w);
    const float mis_weight_camera = mis_pow(sample.pdf_emit_w * cos_theta_i / (sample.pdf_direct_w * cos_theta_o)) *
                                    (mis_eta_vm_ + cam_state.dVCM + cam_state.dVC * mis_pow(pdf_rev_w));

    const float mis_weight = algo == ALGO_PT ? 1.0f : (1.0f / (mis_weight_camera + 1.0f + mis_weight_light));

    VCMState s = cam_state;
    s.throughput *= mis_weight * bsdf_value * cos_theta_i * sample.radiance * pdf_lightpick_inv;

    rays_out_shadow.push(ray, s);
}

VCM_TEMPLATE
void VCM_INTEGRATOR::connect(VCMState& cam_state, const Intersection& isect, BSDF* bsdf_cam, MemoryArena& bsdf_arena, RayQueue<VCMState>& rays_out_shadow) {
    // PDF conversion factor from using the vertex cache.
    // Vertex Cache is equivalent to randomly sampling a path with pdf ~ path length and uniformly sampling a vertex on this path.
    const float vc_weight = (light_vertices_.count(cam_state.sample_id) / light_path_count_) / num_connections_;

    // Connect to num_connections_ randomly chosen vertices from the cache.
    for (int i = 0; i < num_connections_; ++i) {
        const auto& light_vertex = light_vertices_.get_connect(cam_state.sample_id, cam_state.rng);

        // Ignore paths that are longer than the specified maximum length.
        if (light_vertex.path_length + cam_state.path_length > max_path_len_)
            continue;

        const auto light_bsdf = light_vertex.isect.mat->get_bsdf(light_vertex.isect, bsdf_arena, true);

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
        const auto bsdf_value_cam = bsdf_cam->eval(isect.out_dir, connect_dir, BSDF_ALL);
        const float pdf_dir_cam_w = bsdf_cam->pdf(isect.out_dir, connect_dir);
        const float pdf_rev_cam_w = bsdf_cam->pdf(connect_dir, isect.out_dir);

        // Evaluate the bsdf at the light vertex.
        const auto bsdf_value_light = light_bsdf->eval(light_vertex.isect.out_dir, -connect_dir, BSDF_ALL);
        const float pdf_dir_light_w = light_bsdf->pdf(light_vertex.isect.out_dir, -connect_dir);
        const float pdf_rev_light_w = light_bsdf->pdf(-connect_dir, light_vertex.isect.out_dir);

        if (pdf_dir_cam_w == 0.0f || pdf_dir_light_w == 0.0f ||
            pdf_rev_cam_w == 0.0f || pdf_rev_light_w == 0.0f)
            continue;  // A pdf value of zero means that there has to be zero contribution from this pair of directions as well.

        // Compute the cosine terms. We need to use the adjoint for the light vertex BSDF.
        const float cos_theta_cam   = fabsf(dot(isect.normal, connect_dir));
        const float cos_theta_light = fabsf(shading_normal_adjoint(light_vertex.isect.normal, light_vertex.isect.geom_normal,
                                                                   light_vertex.isect.out_dir, -connect_dir));

        const float geom_term = cos_theta_cam * cos_theta_light / connect_dist_sq;
        if (geom_term <= 0.0f)
            continue;

        // Compute and convert the pdfs
        const float pdf_cam_a = pdf_dir_cam_w * cos_theta_light / connect_dist_sq;
        const float pdf_light_a = pdf_dir_light_w * cos_theta_cam / connect_dist_sq;

        // Compute the full MIS weight from the partial weights and pdfs.
        const float mis_weight_light = mis_pow(pdf_cam_a) * (mis_eta_vm_ + light_vertex.dVCM + light_vertex.dVC * mis_pow(pdf_rev_light_w));
        const float mis_weight_camera = mis_pow(pdf_light_a) * (mis_eta_vm_ + cam_state.dVCM + cam_state.dVC * mis_pow(pdf_rev_cam_w));

        const float mis_weight = 1.0f / (mis_weight_camera + 1.0f + mis_weight_light);

        VCMState s = cam_state;
        s.throughput *= vc_weight * mis_weight * geom_term * bsdf_value_cam * bsdf_value_light * light_vertex.throughput;

        Ray ray {
            { isect.pos.x, isect.pos.y, isect.pos.z, offset },
            { connect_dir.x, connect_dir.y, connect_dir.z, connect_dist - offset }
        };

        rays_out_shadow.push(ray, s);
    }
}

VCM_TEMPLATE
void VCM_INTEGRATOR::vertex_merging(const VCMState& state, const Intersection& isect, const BSDF* bsdf, AtomicImage& img) {
    // Obtain the thread local photon buffer and reserve sufficient memory.
    auto& photons = photon_containers.local();
    photons.reserve(0.5f * light_path_count_);
    photons.clear();

    light_vertices_.get_merge(state.sample_id, isect.pos, photons);

    rgb contrib(0.0f);
    const float radius_sqr = pm_radius_ * pm_radius_;
    for (const auto& p : photons) {
        const auto photon_in_dir = p->out_dir;

        const auto bsdf_value = bsdf->eval(isect.out_dir, photon_in_dir);
        const float pdf_dir_w = bsdf->pdf(isect.out_dir, photon_in_dir);
        const float pdf_rev_w = bsdf->pdf(photon_in_dir, isect.out_dir);

        if (pdf_dir_w == 0.0f || pdf_rev_w == 0.0f || is_black(bsdf_value))
            continue;

        // Compute MIS weight.
        const float mis_weight_light = p->dVCM * mis_eta_vc_ + p->dVM * mis_pow(pdf_dir_w);
        const float mis_weight_camera = state.dVCM * mis_eta_vc_ + state.dVM * mis_pow(pdf_rev_w);

        const float mis_weight = algo == ALGO_PPM ? 1.0f : (1.0f / (mis_weight_light + 1.0f + mis_weight_camera));

        // Epanechnikov filter
        const float kernel = 1.0f - dot(isect.pos - p->position, isect.pos - p->position) / radius_sqr;

        contrib += mis_weight * bsdf_value * kernel * p->throughput;
    }

    add_contribution(img, state.pixel_id, state.throughput * contrib * vm_normalization_ * 2.0f); // Factor 2.0f is part of the Epanechnikov filter
}

VCM_TEMPLATE
void VCM_INTEGRATOR::process_shadow_rays(RayQueue<VCMState>& rays_in, AtomicImage& img) {
    const VCMState* states = rays_in.states();
    const Hit* hits = rays_in.hits();

    tbb::parallel_for(tbb::blocked_range<int>(0, rays_in.size()),
        [&] (const tbb::blocked_range<int>& range)
    {
        for (auto i = range.begin(); i != range.end(); ++i)
            if (hits[i].tri_id < 0) // Nothing was hit, the light is visible.
                add_contribution(img, states[i].pixel_id, states[i].throughput);
    });
}

// Explicit instantiations
template class VCMIntegrator<ALGO_PPM>;
template class VCMIntegrator<ALGO_PT >;
template class VCMIntegrator<ALGO_LT >;
template class VCMIntegrator<ALGO_BPT>;
template class VCMIntegrator<ALGO_VCM>;

} // namespace imba
