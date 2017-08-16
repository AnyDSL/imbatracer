#include "imbatracer/render/integrators/deferred_vcm.h"

#define TBB_USE_EXCEPTIONS 0
#include <tbb/enumerable_thread_specific.h>
#include <future>

namespace imba {

// #define STATISTICS
#ifdef STATISTICS
#define PROFILE(cmd, name)  {auto time_start = std::chrono::high_resolution_clock::now(); \
                            cmd; \
                            auto time_end = std::chrono::high_resolution_clock::now(); \
                            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count(); \
                            std::cout << name << "\t-\t" << delta << "ms" << std::endl;}
#else
#define PROFILE(cmd, name) {cmd;}
#endif

template <>
void DeferredVCM<mis::MisVCM>::render(AtomicImage& img) {
    begin_iteration();

    auto m = std::async(std::launch::async, [this, &img] {
        PROFILE(merge(img), "Merge");
    });

    auto lt = std::async(std::launch::async, [this, &img] {
        PROFILE(light_tracing(img), "LT");
    });

    auto conn = std::async(std::launch::async, [this, &img] {
        PROFILE(path_tracing(img, true), "PT");
        PROFILE(connect(img), "Connect");
    });

    lt.wait();
    conn.wait();
    m.wait();

    end_iteration();
}

template <>
void DeferredVCM<mis::MisBPT>::render(AtomicImage& img) {
    begin_iteration();

    auto lt = std::async(std::launch::async, [this, &img] {
        PROFILE(light_tracing(img), "LT");
    });

    auto conn = std::async(std::launch::async, [this, &img] {
        PROFILE(path_tracing(img, true), "PT");
        PROFILE(connect(img), "Connect");
    });

    lt.wait();
    conn.wait();

    end_iteration();
}

template <>
void DeferredVCM<mis::MisPT>::render(AtomicImage& img) {
    begin_iteration();

    PROFILE(path_tracing(img, true), "PT");

    end_iteration();
}

template <>
void DeferredVCM<mis::MisLT>::render(AtomicImage& img) {
    begin_iteration();

    PROFILE(light_tracing(img), "LT");

    end_iteration();
}

template <>
void DeferredVCM<mis::MisTWPT>::render(AtomicImage& img) {
    begin_iteration();

    auto pt = std::async(std::launch::async, [this, &img] {
        PROFILE(path_tracing(img, true), "PT");
    });

    auto lt = std::async(std::launch::async, [this, &img] {
        PROFILE(light_tracing(img), "LT");
    });

    pt.wait();
    lt.wait();

    end_iteration();
}

template <>
void DeferredVCM<mis::MisSPPM>::render(AtomicImage& img) {
    begin_iteration();

    auto pt = std::async(std::launch::async, [this, &img] {
        PROFILE(path_tracing(img, true), "PT");
    });

    auto m = std::async(std::launch::async, [this, &img] {
        PROFILE(merge(img), "Merge");
    });

    pt.wait();
    m.wait();

    end_iteration();
}

template <typename MisType>
void DeferredVCM<MisType>::begin_iteration() {
    const float radius_alpha = 0.75f;
    cur_iteration_++;
    pm_radius_ = base_radius_ / powf(static_cast<float>(cur_iteration_), 0.5f * (1.0f - radius_alpha));
    pm_radius_ = std::max(pm_radius_, 1e-7f); // ensure numerical stability
    merge_pdf_ = mis::merge_accept_weight(settings_.light_path_count, pm_radius_);

    cam_verts_[cur_cache_]->clear();
    light_verts_[cur_cache_]->clear();

    PROFILE(trace(), "Tracing camera and light paths");

    auto pm_task = std::async(std::launch::async, [this] {
        PROFILE(photon_grid_.build(light_verts_[cur_cache_]->begin(),
                                   light_verts_[cur_cache_]->end(),
                                   pm_radius_,
                                   [](auto& v){ return !v.specular; }), "Building hash grid");
    });

    auto im_task = std::async(std::launch::async, [this] {
        PROFILE(importon_grid_.build(cam_verts_[cur_cache_]->begin(),
                                     cam_verts_[cur_cache_]->end(),
                                     base_radius_,
                                     [](auto& v){ return !v.specular; }), "Building hash grid (importons)");
    });

#ifdef PATH_STATISTICS
    dump_vertices("camera_paths.path", settings_.width * settings_.height * settings_.concurrent_spp,
                  cam_verts_[cur_cache_]->begin(), cam_verts_[cur_cache_]->end(),
                  [] (auto& v) {
                      return DebugVertex(v.state.throughput, v.isect, v.state.pixel_id, v.state.ancestor,
                                         v.state.path_length, v.specular);
                  });

    dump_vertices("light_paths.path", settings_.light_path_count,
                  light_verts_[cur_cache_]->begin(), light_verts_[cur_cache_]->end(),
                  [] (auto& v) {
                      return DebugVertex(v.state.throughput, v.isect, v.state.light_id, v.state.ancestor,
                                         v.state.path_length, v.specular);
                  });

    path_log_.enable(PathDebugger<Vertex>::merging);
#endif

    pm_task.wait();
    im_task.wait();
}

template <typename MisType>
void DeferredVCM<MisType>::end_iteration() {
    cur_cache_ = (cur_cache_ + 1) % 2;

#ifdef PATH_STATISTICS
    path_log_.write("pathdbg.obj");
#endif
}

template <typename MisType>
void DeferredVCM<MisType>::compute_local_stats(VertCache& from, int offset, const HashGrid<VertexHandle>& accel, int num_paths) {
    tbb::parallel_for(tbb::blocked_range<int>(offset, from.size()),
        [&] (const tbb::blocked_range<int>& range)
    {
        for (auto i = range.begin(); i != range.end(); ++i) {
            auto& v = from[i];

            MaterialValue mat;
            scene_.material_system()->eval_material(v.isect, false, mat);
            mat.bsdf.prepare(v.state.throughput, v.isect.out_dir);
            if (mat.bsdf.is_specular()) continue;
            const auto bsdf = &mat.bsdf;

            const int k = settings_.num_knn;
            auto records = V_ARRAY(VertexHandle, k);
            int count = accel.query(v.isect.pos, records, k);
            const float radius_sqr = (count == k) ? lensqr(records[k - 1].vert->isect.pos - v.isect.pos) : (pm_radius_ * pm_radius_);

            rgb contrib(0.0f);
            float mis = 1.0f;
            for (int i = 0; i < count; ++i) {
                auto p = records[i].vert;
                if (p->state.path_length < 2) continue; // Do not consider particles on the emitter (if they were generated there)

                const auto& in_dir = p->isect.out_dir;

                const auto& bsdf_value = bsdf->eval(v.isect.out_dir, in_dir);
                const float pdf_dir_w = bsdf->pdf(v.isect.out_dir, in_dir);
                const float pdf_rev_w = bsdf->pdf(in_dir, v.isect.out_dir);

                if (pdf_dir_w == 0.0f || pdf_rev_w == 0.0f || is_black(bsdf_value))
                    continue;

                const float mis_weight = mis::weight_merge(v.state.mis, p->state.mis, merge_pdf_, pdf_dir_w, pdf_rev_w,
                                                           v.state.path_length, p->state.path_length);
                mis *= mis_weight;

                // Epanechnikov filter
                const float d = lensqr(p->isect.pos - v.isect.pos);
                const float kernel = 1.0f - d / radius_sqr;

                // Apparent density changes
                const float adjoint = 1.0f / fabsf(dot(in_dir, p->isect.geom_normal));

                contrib += bsdf_value * adjoint * kernel * p->state.throughput;
            }

            // Complete the Epanechnikov kernel
            contrib *= v.state.throughput * 2.0f / (pi * radius_sqr * num_paths);

            v.density.contrib    = contrib;
            v.density.mis_weight = mis;
        }
    });
}

template <typename MisType>
void DeferredVCM<MisType>::trace_camera_primary() {
    typename QueueScheduler<State>::ShadeEmptyFn env_hit;
    if (scene_.env_map())
        env_hit = [this] (Ray& r, State& s) { process_envmap_hits(r, s); };

    const int prim_rays = settings_.width * settings_.height * settings_.concurrent_spp;

    cam_verts_[cur_cache_]->grow(prim_rays);
    scheduler_.resize(prim_rays);

    scheduler_.start(&scene_, false, [this] (Ray& r, Hit& h, State& s) {
        process_hits(r, h, s);
    }, env_hit);

    // Generate the primary rays from the camera
    TBB_PAR_FOR_BEGIN(0, prim_rays)
    const int batch_size = range.end() - range.begin();
    auto rays   = V_ARRAY(Ray,   batch_size);
    auto states = V_ARRAY(State, batch_size);
    for (auto i = range.begin(); i != range.end(); ++i) {
        int pixel_idx = i / settings_.concurrent_spp;
        int y = pixel_idx / settings_.width;
        int x = pixel_idx % settings_.width;

        State state;
        state.pixel_id    = pixel_idx;
        state.rng         = RNG(bernstein_seed(cur_iteration_, x * settings_.height + y));
        state.throughput  = rgb(1.0f);
        state.path_length = 1;
        state.ancestor    = -1;
        state.adjoint     = false;

        auto ray = cam_.generate_ray(x + state.rng.random_float(), y + state.rng.random_float());
        const float pdf = cam_.pdf(ray.dir);

        state.mis.init_camera(settings_.light_path_count, pdf);

        rays  [i - range.begin()] = ray;
        states[i - range.begin()] = state;
    }
    scheduler_.push(rays, states, batch_size);
    TBB_PAR_FOR_END;

    scheduler_.flush();
}

template <typename MisType>
void DeferredVCM<MisType>::trace_light_primary() {
    const int prim_rays = settings_.light_path_count;

    light_verts_[cur_cache_]->grow(prim_rays);
    scheduler_.resize(prim_rays);

    scheduler_.start(&scene_, false, [this] (Ray& r, Hit& h, State& s) {
        process_hits(r, h, s);
    }, nullptr);

    // TODO add importance sampling based on
    //      (1) total light source power
    //      (2) product of total light soruce power and total light source importance

    // Generate the primary rays from the light sources.
    TBB_PAR_FOR_BEGIN(0, prim_rays)
    const int batch_size = range.end() - range.begin();
    auto rays   = V_ARRAY(Ray,   batch_size);
    auto states = V_ARRAY(State, batch_size);
    for (auto i = range.begin(); i != range.end(); ++i) {
        State state;
        state.rng      = RNG(bernstein_seed(cur_iteration_, i));

        // Select a light source (TODO importance sampling)
        int light_id = state.rng.random_int(0, scene_.light_count());

        state.light_id = light_id;
        auto& l = scene_.light(light_id);

        float pdf_lightpick = 1.0f / scene_.light_count();

        // Sample a ray and initialize its contribution.
        Light::EmitSample sample = l->sample_emit(state.rng);

        Ray ray;
        ray.org = make_vec4(sample.isect.pos, 1e-4f);
        ray.dir = make_vec4(sample.dir, FLT_MAX);

        state.throughput  = sample.radiance / pdf_lightpick;
        state.path_length = 1;
        state.ancestor    = -1;
        state.ancestor    = light_verts_[cur_cache_]->add(Vertex(state, sample.isect, l->is_delta() || !l->is_finite()));
        state.adjoint     = true;
        state.mis.init_light(sample.pdf_emit_w, sample.pdf_direct_a, pdf_lightpick, sample.cos_out, l->is_finite(), l->is_delta());

        rays  [i - range.begin()] = ray;
        states[i - range.begin()] = state;
    }
    scheduler_.push(rays, states, batch_size);
    TBB_PAR_FOR_END;

    scheduler_.flush();
}

template <typename MisType>
void DeferredVCM<MisType>::trace() {
    const int num_cam_paths   = settings_.width * settings_.height * settings_.concurrent_spp;
    const int num_light_paths = settings_.light_path_count;
    const float wave_rr_pdf = 0.8f;
    const int num_continuations = settings_.width * settings_.height;

    trace_camera_primary();
    trace_light_primary ();

    // Create a global random number generator for wavefront termination and vertex selection / filtering.
    RNG wave_rng(bernstein_seed(42, cur_iteration_));

    // Trace the continuation rays.
    int vert_offset_cam   = 0;
    int vert_offset_light = 0;

    // TODO no need to allocate these once per iteration, move to class members.
    std::vector<float> vertex_pmf;
    std::vector<float> vertex_cdf;
    std::vector<int>   continue_mask;

    scheduler_.resize(num_continuations * 2);

    for (int wave_depth = 0; wave_depth < settings_.max_path_len; ++wave_depth) {
        // Randomly decide to kill the entire wavefront.
        if (wave_rng.random_float() > wave_rr_pdf) break;

        // Also terminate if not a single ray hit anything.
        if (vert_offset_cam   == cam_verts_  [cur_cache_]->size() &&
            vert_offset_light == light_verts_[cur_cache_]->size()) break;

        if (cur_iteration_ > 1) {
            compute_local_stats(  *cam_verts_[cur_cache_],   vert_offset_cam,   photon_grid_, num_light_paths);
            compute_local_stats(*light_verts_[cur_cache_], vert_offset_light, importon_grid_,   num_cam_paths);
        }

        int num_verts_cam   =   cam_verts_[cur_cache_]->size() - vert_offset_cam;
        int num_verts_light = light_verts_[cur_cache_]->size() - vert_offset_light;
        int num_verts = num_verts_cam + num_verts_light;

        // Ensure that the buffers are large enough
        if (vertex_pmf.size() < num_verts) {
            vertex_pmf   .resize(num_verts);
            vertex_cdf   .resize(num_verts);
            continue_mask.resize(num_verts);
        }

        // Build the PMF in parallel for the camera and light vertices.
        float pmf_sum = tbb::parallel_reduce(tbb::blocked_range<int>(0, num_verts), 0.0f,
            [&] (const tbb::blocked_range<int>& range, float init) {
                for (auto i = range.begin(); i != range.end(); ++i) {
                    auto& vert = i >= num_verts_cam
                               ? (*light_verts_[cur_cache_])[i - num_verts_cam + vert_offset_light]
                               : (  *cam_verts_[cur_cache_])[i + vert_offset_cam  ];

                    // If no adjoint information is available, assume Li = 1 everywhere
                    // TODO add special case for specular
                    if (cur_iteration_ > 1) {
                        vertex_pmf[i] = is_black (vert.density.contrib)
                                      ? luminance(vert.state.throughput)
                                      : luminance(vert.density.contrib);
                    } else
                        vertex_pmf[i] = luminance(vert.state.throughput);

                    init += vertex_pmf[i];
                }
                return init;
            },
            [] (float a, float b) { return a + b; });

        // Normalize PMF and initialize selection.
        tbb::parallel_for(tbb::blocked_range<int>(0, num_verts),
            [&] (const tbb::blocked_range<int>& range) {
                for (auto i = range.begin(); i != range.end(); ++i) {
                    vertex_pmf   [i] /= pmf_sum;
                    continue_mask[i]  = 1;//0;
                }
            });

        std::partial_sum(vertex_pmf.begin(), vertex_pmf.begin() + num_verts, vertex_cdf.begin());

        // Select N vertices to continue with replacement.
        tbb::parallel_for(tbb::blocked_range<int>(0, num_continuations),
            [&] (const tbb::blocked_range<int>& range) {
                for (auto i = range.begin(); i != range.end(); ++i) {
                    auto idx = std::lower_bound(vertex_cdf.begin(), vertex_cdf.begin() + num_verts, wave_rng.random_float());
                    int k = idx - vertex_cdf.begin();
                    if (k >= num_verts) k = num_verts - 1;
                    // continue_mask[k]++;
                }
            });

        int old_sz_cam   =   cam_verts_[cur_cache_]->size();
        int old_sz_light = light_verts_[cur_cache_]->size();

        // Ensure that both vertex caches are large enough
        cam_verts_  [cur_cache_]->grow(old_sz_cam   + num_continuations + num_verts);
        light_verts_[cur_cache_]->grow(old_sz_light + num_continuations + num_verts);

        // TODO special case for specular (always continued with one ray, terminated with the wavefront, never split)

        // Trace continuation rays from the selected vertices.
        scheduler_.start(&scene_, false, [this] (Ray& r, Hit& h, State& s) {
            process_hits(r, h, s);
        }, [this] (Ray& r, State& s) {
            if (s.adjoint)
                process_envmap_hits(r, s);
        });

        TBB_PAR_FOR_BEGIN(0, num_verts)
        const int batch_size = range.end() - range.begin();
        auto rays   = V_ARRAY(Ray,   batch_size);
        auto states = V_ARRAY(State, batch_size);
        int cur = 0;
        for (auto i = range.begin(); i != range.end(); ++i) {
            // Determine the index of the vertex and the number of continuation rays for that one.
            const int num_c    = continue_mask[i];

            if (!num_c) continue; // This vertex was not selected

            const bool adjoint = i >= num_verts_cam;
            const int vert_idx = adjoint
                               ? i - num_verts_cam + vert_offset_light
                               : i + vert_offset_cam;
            auto& vert = adjoint
                       ? (*light_verts_[cur_cache_])[vert_idx]
                       : (  *cam_verts_[cur_cache_])[vert_idx];

            const float pdf_select = vertex_pmf[i];
            const float rr_pdf = wave_rr_pdf * pdf_select / num_c;
            const float offset = sqrtf(vert.isect.d_sqr) * 1e-4f;

            MaterialValue mat;
            scene_.material_system()->eval_material(vert.isect, adjoint, mat);
            mat.bsdf.prepare(vert.state.throughput, vert.isect.out_dir);

            // Sample all continuation rays and add them to the buffer.
            for (int k = 0; k < num_c; ++k) {
                Ray ray;
                State state = vert.state;
                state.ancestor = vert_idx;

                bounce(state, vert.isect, &mat.bsdf, ray, offset, rr_pdf);

                // If the buffer is full, push it.
                if (cur == batch_size) {
                    scheduler_.push(rays, states, cur);
                    cur = 0;
                }

                // Add the ray and its state to the buffer.
                rays  [cur] = ray;
                states[cur] = state;
                cur++;
            }
        }
        scheduler_.push(rays, states, cur);
        TBB_PAR_FOR_END;

        scheduler_.flush();

        vert_offset_cam   = old_sz_cam;
        vert_offset_light = old_sz_light;
    }
}

template <typename MisType>
void DeferredVCM<MisType>::bounce(State& state, const Intersection& isect, BSDF* bsdf, Ray& ray, float offset, float rr_pdf) {
    float pdf_dir_w;
    float3 sample_dir;
    bool specular;
    auto bsdf_value = bsdf->sample(isect.out_dir, sample_dir, state.rng, pdf_dir_w, specular);

    if (pdf_dir_w == 0.0f || is_black(bsdf_value)) {
        terminate_path(state);
        return;
    }

    float pdf_rev_w = 0.0f;
    if (!specular)
        pdf_rev_w = bsdf->pdf(sample_dir, isect.out_dir);

    const float cos_theta_i = fabsf(dot(sample_dir, isect.geom_normal));

    state.throughput *= bsdf_value / rr_pdf;
    state.mis.update_bounce(pdf_dir_w, pdf_rev_w, cos_theta_i, specular, merge_pdf_, state.path_length, !state.adjoint);

    ray = Ray {
        { isect.pos.x, isect.pos.y, isect.pos.z, offset },
        { sample_dir.x, sample_dir.y, sample_dir.z, FLT_MAX }
    };
}

template <typename MisType>
void DeferredVCM<MisType>::process_envmap_hits(Ray& r, State& state) {
    // TODO
    // The environment map was "hit"
    // I should record this hit somehow, to
    //   a) help guiding environment map sampling
    //   b) do the environment map hit evaluation in a deferred way as well, consistent with the rest of the algorithm
}

template <typename MisType>
void DeferredVCM<MisType>::process_hits(Ray& r, Hit& h, State& state) {
    const auto isect = scene_.calculate_intersection(h, r);
    const float cos_theta_o = fabsf(dot(isect.out_dir, isect.geom_normal));

    if (cos_theta_o == 0.0f) { // Prevent NaNs
        terminate_path(state);
        return;
    }

    MaterialValue mat;
    scene_.material_system()->eval_material(isect, state.adjoint, mat);
    mat.bsdf.prepare(state.throughput, isect.out_dir);

    state.mis.update_hit(cos_theta_o, h.tmax * h.tmax);
    state.path_length++;

    VertCache* cache = state.adjoint ? light_verts_[cur_cache_].get() : cam_verts_[cur_cache_].get();

    state.ancestor = cache->add(Vertex(state, isect, mat.bsdf.is_specular()));

    terminate_path(state);
}

template <typename MisType>
void DeferredVCM<MisType>::path_tracing(AtomicImage& img, bool next_evt) {
    shadow_scheduler_pt_.resize(cam_verts_[cur_cache_]->size());

    shadow_scheduler_pt_.start(&scene_, true, nullptr, [this, &img] (Ray& r, ShadowState& s) {
        add_contribution(img, s.pixel_id, s.contrib);
    });

    TBB_PAR_FOR_BEGIN(0, cam_verts_[cur_cache_]->size())
    const int batch_size = range.end() - range.begin();
    auto rays   = V_ARRAY(Ray,         batch_size);
    auto states = V_ARRAY(ShadowState, batch_size);
    int generated = 0;
    for (auto i = range.begin(); i != range.end(); ++i) {
        auto& v = (*cam_verts_[cur_cache_])[i];

        MaterialValue mat;
        scene_.material_system()->eval_material(v.isect, false, mat);
        mat.bsdf.prepare(v.state.throughput, v.isect.out_dir);

        if (mat.bsdf.is_specular()) continue;

        if (!is_black(mat.emit)) {
            float cos_out = dot(v.isect.geom_normal, v.isect.out_dir);
            if (cos_out < 0.0f) continue;

            float pdf_lightpick = 1.0f / scene_.light_count();
            float pdf_direct_a  = 1.0f / v.isect.area;
            float pdf_emit_w    = 1.0f / v.isect.area * cos_hemisphere_pdf(cos_out);

            const float mis_weight = mis::weight_upt(v.state.mis, merge_pdf_, pdf_direct_a, pdf_emit_w, pdf_lightpick, v.state.path_length);

            rgb color = v.state.throughput * mat.emit * mis_weight;
            add_contribution(img, v.state.pixel_id, color);

            continue;
        } else if (!next_evt)
            continue;

        // Sample a point on a light
        const auto& ls = scene_.light(v.state.rng.random_int(0, scene_.light_count()));
        const float pdf_lightpick_inv = scene_.light_count();
        const auto sample = ls->sample_direct(v.isect.pos, v.state.rng);
        const float cos_theta_i = fabsf(dot(v.isect.geom_normal, sample.dir));

        // Evaluate the BSDF and compute the pdf values
        auto bsdf = &mat.bsdf;
        auto bsdf_value = bsdf->eval(v.isect.out_dir, sample.dir);
        float pdf_dir_w = bsdf->pdf(v.isect.out_dir, sample.dir);
        float pdf_rev_w = bsdf->pdf(sample.dir, v.isect.out_dir);

        if (pdf_dir_w == 0.0f || pdf_rev_w == 0.0f)
           continue;

        const float mis_weight = mis::weight_di(v.state.mis, merge_pdf_, pdf_dir_w, pdf_rev_w,
                                                sample.pdf_direct_w, sample.pdf_emit_w, pdf_lightpick_inv,
                                                cos_theta_i, sample.cos_out, ls->is_delta(), v.state.path_length);

        const float offset = 1e-3f * (sample.distance == FLT_MAX ? 1.0f : sample.distance);

        Ray ray;
        ray.org = make_vec4(v.isect.pos, offset);
        ray.dir = make_vec4(sample.dir, sample.distance - offset);

        ShadowState state;
        state.contrib  = v.state.throughput * bsdf_value * sample.radiance * mis_weight * pdf_lightpick_inv;
        state.pixel_id = v.state.pixel_id;

        int idx = generated++;
        rays  [idx] = ray;
        states[idx] = state;
    }
    shadow_scheduler_pt_.push(rays, states, generated);
    TBB_PAR_FOR_END;

    shadow_scheduler_pt_.flush();
}

template <typename MisType>
void DeferredVCM<MisType>::light_tracing(AtomicImage& img) {
    shadow_scheduler_lt_.resize(light_verts_[cur_cache_]->size());

    shadow_scheduler_lt_.start(&scene_, true, nullptr, [this, &img] (Ray& r, ShadowState& s) {
        add_contribution(img, s.pixel_id, s.contrib);
    });

    // Generate the primary rays from the lights
    TBB_PAR_FOR_BEGIN(0, light_verts_[cur_cache_]->size())
    const int batch_size = range.end() - range.begin();
    auto rays   = V_ARRAY(Ray,         batch_size);
    auto states = V_ARRAY(ShadowState, batch_size);
    int generated = 0;
    for (auto i = range.begin(); i != range.end(); ++i) {
        const auto& v = (*light_verts_[cur_cache_])[i];

        if (v.state.path_length == 1)
            continue; // Do not connect vertices on the light source itself

        float3 dir_to_cam = cam_.pos() - v.isect.pos;

        if (dot(-dir_to_cam, cam_.dir()) < 0.0f)
            continue; // Vertex is behind the camera.

        const float2 raster_pos = cam_.world_to_raster(v.isect.pos);

        ShadowState state;
        state.pixel_id = cam_.raster_to_id(raster_pos);

        if (state.pixel_id < 0 || state.pixel_id >= settings_.width * settings_.height)
            continue; // The point is outside the image plane.

        // Compute ray direction and distance.
        const float dist_to_cam_sqr = lensqr(dir_to_cam);
        const float dist_to_cam = sqrtf(dist_to_cam_sqr);
        dir_to_cam = dir_to_cam / dist_to_cam;
        const float cos_theta_surf = fabsf(dot(v.isect.geom_normal, dir_to_cam));

        float pdf_cam = cam_.pdf(-dir_to_cam);
        pdf_cam *= 1.0f / dist_to_cam_sqr;

        // Evaluate the BSDF and compute the pdf values
        MaterialValue mat;
        scene_.material_system()->eval_material(v.isect, true, mat);
        mat.bsdf.prepare(v.state.throughput, v.isect.out_dir);

        if (mat.bsdf.is_specular()) continue;

        auto bsdf = &mat.bsdf;
        auto bsdf_value = bsdf->eval(v.isect.out_dir, dir_to_cam);
        float pdf_rev_w = bsdf->pdf(dir_to_cam, v.isect.out_dir);

        if (pdf_rev_w == 0.0f) continue;

        const float mis_weight = mis::weight_lt(v.state.mis, merge_pdf_, pdf_cam * cos_theta_surf, pdf_rev_w,
                                                settings_.light_path_count, v.state.path_length);

        const float offset = dist_to_cam * 1e-4f;

        Ray ray;
        ray.org = make_vec4(v.isect.pos, offset);
        ray.dir = make_vec4(dir_to_cam, dist_to_cam - offset);

        state.contrib = v.state.throughput * bsdf_value * pdf_cam * mis_weight / settings_.light_path_count;

        int idx = generated++;
        rays  [idx] = ray;
        states[idx] = state;
    }
    shadow_scheduler_lt_.push(rays, states, generated);
    TBB_PAR_FOR_END;

    shadow_scheduler_lt_.flush();
}

template <typename MisType>
void DeferredVCM<MisType>::connect(AtomicImage& img) {
    // TODO: build / reuse a PMF to select what vertices to connect from (and how many connections to do)

    shadow_scheduler_connect_.resize(cam_verts_[cur_cache_]->size() * settings_.num_connections);

    shadow_scheduler_connect_.start(&scene_, true, nullptr, [this, &img] (Ray& r, ShadowState& s) {
        add_contribution(img, s.pixel_id, s.contrib);

#ifdef PATH_STATISTICS
        const auto& cam_v   = *cam_verts_[cur_cache_];
        const auto& light_v = *light_verts_[cur_cache_];
        if (s.mis_weight > 0.9f) {
            path_log_.log_connection(*s.cam, *s.light, [&, this](Vertex& v) {
                if (v.state.ancestor < 0) return false;
                v = cam_v[v.state.ancestor];
                return true;
            }, [&, this](Vertex& v){
                if (v.state.ancestor < 0) return false;
                v = light_v[v.state.ancestor];
                return true;
            }, [this](Vertex& v){
                return v.isect.pos;
            });
        }
#endif
    });

    TBB_PAR_FOR_BEGIN(0, cam_verts_[cur_cache_]->size() * settings_.num_connections)
    const int batch_size = range.end() - range.begin();
    auto rays   = V_ARRAY(Ray,         batch_size);
    auto states = V_ARRAY(ShadowState, batch_size);
    int generated = 0;
    for (auto i = range.begin(); i != range.end(); ++i) {
        auto& cam_v   = *cam_verts_[cur_cache_];
        const auto& light_v = *light_verts_[cur_cache_];
        auto& v = cam_v[i];

        // PDF conversion factor from using the vertex cache.
        // Vertex Cache is equivalent to randomly sampling a path with pdf ~ path length and uniformly sampling a vertex on this path.
        const float vc_weight = light_v.size() / (float(settings_.light_path_count) * float(settings_.num_connections));

        int lv_idx = v.state.rng.random_int(0, light_v.size());
        auto& light_vertex = light_v[lv_idx];
        if (light_vertex.state.path_length == 1) // do not connect to the light (handled by next event!)
            continue;

        MaterialValue lmat;
        scene_.material_system()->eval_material(light_vertex.isect, true, lmat);
        lmat.bsdf.prepare(light_vertex.state.throughput, light_vertex.isect.out_dir);

        MaterialValue cmat;
        scene_.material_system()->eval_material(v.isect, false, cmat);
        cmat.bsdf.prepare(v.state.throughput, v.isect.out_dir);

        if (cmat.bsdf.is_specular() || lmat.bsdf.is_specular()) continue;

        const auto light_bsdf = &lmat.bsdf;
        const auto cam_bsdf   = &cmat.bsdf;

        // Compute connection direction and distance.
        float3 connect_dir = light_vertex.isect.pos - v.isect.pos;
        const float connect_dist_sq = lensqr(connect_dir);
        const float connect_dist = std::sqrt(connect_dist_sq);
        connect_dir *= 1.0f / connect_dist;

        if (connect_dist < base_radius_) {
            // If two points are too close to each other, they are either occluded or have cosine terms
            // that are close to zero. Numerical inaccuracies might yield an overly bright pixel.
            // The correct result is usually black or close to black so we just ignore those connections.

            // TODO: check if this makes any sense, maybe adapt MIS weights for this case to get rid of bias!
            continue;
        }

        // Evaluate the bsdf at the camera vertex.
        const auto bsdf_value_cam = cam_bsdf->eval(v.isect.out_dir, connect_dir);
        const float pdf_dir_cam_w = cam_bsdf->pdf (v.isect.out_dir, connect_dir);
        const float pdf_rev_cam_w = cam_bsdf->pdf (connect_dir, v.isect.out_dir);

        // Evaluate the bsdf at the light vertex.
        const auto bsdf_value_light = light_bsdf->eval(light_vertex.isect.out_dir, -connect_dir);
        const float pdf_dir_light_w = light_bsdf->pdf(light_vertex.isect.out_dir, -connect_dir);
        const float pdf_rev_light_w = light_bsdf->pdf(-connect_dir, light_vertex.isect.out_dir);

        if (pdf_dir_cam_w == 0.0f || pdf_dir_light_w == 0.0f ||
            pdf_rev_cam_w == 0.0f || pdf_rev_light_w == 0.0f)
            continue;  // A pdf value of zero means that there has to be zero contribution from this pair of directions as well.

        // Compute the cosine terms. We need to use the adjoint for the light vertex BSDF.
        const float cos_theta_cam   = fabsf(dot(v.isect.geom_normal, connect_dir));
        const float cos_theta_light = fabsf(dot(light_vertex.isect.geom_normal, -connect_dir));

        const float geom_term = 1.0f / connect_dist_sq; // Cosine contained in bsdf

        const float mis_weight = mis::weight_connect(v.state.mis, light_vertex.state.mis, merge_pdf_, pdf_dir_cam_w, pdf_rev_cam_w,
                                                     pdf_dir_light_w, pdf_rev_light_w,
                                                     cos_theta_cam, cos_theta_light, connect_dist_sq,
                                                     v.state.path_length, light_vertex.state.path_length);

        ShadowState state;
        state.pixel_id = v.state.pixel_id;
        state.contrib  = v.state.throughput * vc_weight * mis_weight * geom_term
                       * bsdf_value_cam * bsdf_value_light * light_vertex.state.throughput;

#ifdef PATH_STATISTICS
        state.cam = &v;
        state.light = &light_vertex;
        state.mis_weight = mis_weight;
#endif

        const float offset = 1e-4f * connect_dist;
        Ray ray;
        ray.org = make_vec4(v.isect.pos, offset);
        ray.dir = make_vec4(connect_dir, connect_dist - offset);

        int idx = generated++;
        rays  [idx] = ray;
        states[idx] = state;
    }
    shadow_scheduler_connect_.push(rays, states, generated);
    TBB_PAR_FOR_END;

    shadow_scheduler_connect_.flush();
}

template <typename MisType>
void DeferredVCM<MisType>::merge(AtomicImage& img) {
    const auto& cam_v = *cam_verts_[cur_cache_];

    tbb::parallel_for(tbb::blocked_range<int>(0, cam_verts_[cur_cache_]->size()),
        [&] (const tbb::blocked_range<int>& range)
    {
        for (auto i = range.begin(); i != range.end(); ++i) {
            const auto& v = cam_v[i];

            MaterialValue mat;
            scene_.material_system()->eval_material(v.isect, false, mat);
            mat.bsdf.prepare(v.state.throughput, v.isect.out_dir);
            if (mat.bsdf.is_specular()) continue;
            const auto bsdf = &mat.bsdf;

            const int k = settings_.num_knn;
            auto photons = V_ARRAY(VertexHandle, k);
            int count = photon_grid_.query(v.isect.pos, photons, k);
            const float radius_sqr = (count == k) ? lensqr(photons[k - 1].vert->isect.pos - v.isect.pos) : (pm_radius_ * pm_radius_);

            rgb contrib(0.0f);
            for (int i = 0; i < count; ++i) {
                auto p = photons[i].vert;
                if (p->state.path_length < 2) continue; // do not merge on the light sources

                const auto& photon_in_dir = p->isect.out_dir;

                const auto& bsdf_value = bsdf->eval(v.isect.out_dir, photon_in_dir);
                const float pdf_dir_w  = bsdf->pdf (v.isect.out_dir, photon_in_dir);
                const float pdf_rev_w  = bsdf->pdf (photon_in_dir, v.isect.out_dir);

                if (pdf_dir_w == 0.0f || pdf_rev_w == 0.0f || is_black(bsdf_value))
                    continue;

                const float mis_weight = mis::weight_merge(v.state.mis, p->state.mis, merge_pdf_, pdf_dir_w, pdf_rev_w,
                                                           v.state.path_length, p->state.path_length);

                // Epanechnikov filter
                const float d = lensqr(p->isect.pos - v.isect.pos);
                const float kernel = 1.0f - d / radius_sqr;

                // Apparent density changes
                const float adjoint = 1.0f / fabsf(dot(photon_in_dir, p->isect.geom_normal));

                contrib += mis_weight * bsdf_value * adjoint * kernel * p->state.throughput;

#ifdef PATH_STATISTICS
                const auto& cam_v   = *cam_verts_[cur_cache_];
                const auto& light_v = *light_verts_[cur_cache_];
                if (mis_weight > 0.9f && p->state.path_length == 2) {
                    path_log_.log_merge(pm_radius_, v, *p, [&, this](Vertex& v) {
                        if (v.state.ancestor < 0) return false;
                        v = cam_v[v.state.ancestor];
                        return true;
                    }, [&, this](Vertex& v){
                        if (v.state.ancestor < 0) return false;
                        v = light_v[v.state.ancestor];
                        return true;
                    }, [this](Vertex& v){
                        return v.isect.pos;
                    });
                }
#endif
            }

            // Complete the Epanechnikov kernel
            contrib *= 2.0f / (pi * radius_sqr * settings_.light_path_count);

            add_contribution(img, v.state.pixel_id, v.state.throughput * contrib);
        }
    });
}

template class DeferredVCM<mis::MisVCM>;
template class DeferredVCM<mis::MisBPT>;
template class DeferredVCM<mis::MisPT>;
template class DeferredVCM<mis::MisLT>;
template class DeferredVCM<mis::MisTWPT>;
template class DeferredVCM<mis::MisSPPM>;

} // namespace imba
