#ifndef IMBA_DEFERRED_VCM_H
#define IMBA_DEFERRED_VCM_H

#include "imbatracer/render/materials/material_system.h"
#include "imbatracer/render/scheduling/deferred_scheduler.h"

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/integrators/deferred_vertices.h"
#include "imbatracer/render/integrators/deferred_mis.h"

#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/rangesearch/rangesearch.h"

#include "imbatracer/frontend/cmd_line.h"

namespace imba {

template <typename MisType>
class DeferredVCM : public Integrator {
    struct State : public RayState {
        /// The power or importance carried by the path up to this intersection
        rgb throughput;

        /// Number of vertices along this path until now (includes vertex at camera / light)
        uint16_t path_length;

        /// Index within the vertex cache where the previous vertex along this path was stored
        /// -1 if the ancestor was not stored (e.g. is on a specular surface, first vertex, cache too small, ...)
        uint16_t ancestor;

        MisType mis;
    };

    struct ShadowState : public RayState {
        /// Weighted contribution of the shadow ray if it is not occluded
        rgb contrib;
    };

    struct Vertex {
        MisType mis;
        rgb throughput; ///< The power or importance of the path leading to this vertex

        Intersection isect;

        union {
            int pixel_id; ///< Id of the pixel from which this path was sampled (only for camera paths)
            int light_id; ///< Id of the light source from which this path was sampled (only for light paths)
        };
        uint32_t ancestor : 24;
        uint32_t path_len : 8;

        Vertex() {}
        Vertex(const MisType& mis, const rgb& c, int a, int pixel, int len, const Intersection& isect)
            : mis(mis), throughput(c), ancestor(a), pixel_id(pixel), path_len(len), isect(isect)
        {}

        // TODO this is used by the emission (can be replaced by the other constructor if light exposes material)
        Vertex(const MisType& mis, const rgb& c, int a, int pixel, int len, const float3& pos)
            : mis(mis), throughput(c), ancestor(a), pixel_id(pixel), path_len(len)
        {
            isect.pos = pos;
        }
    };

    struct VertexHandle {
        VertexHandle() : vert(nullptr) {}

        VertexHandle(const Vertex& v) {
            vert = &v;
        }

        VertexHandle(const Vertex* v) {
            vert = v;
        }

        VertexHandle& operator= (const Vertex& v) {
            vert = &v;
            return *this;
        }

        VertexHandle& operator= (const VertexHandle* v) {
            vert = v->vert;
            return *this;
        }

        const float3& position() const { return vert->isect.pos; }

        const Vertex* vert;
    };

    using VertCache = DeferredVertices<Vertex>;

public:
    DeferredVCM(Scene& scene, PerspectiveCamera& cam, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , cur_iteration_(0)
        , light_tile_gen_(scene.light_count(), settings.light_path_count, settings.tile_size * settings.tile_size)
        , camera_tile_gen_(settings.width, settings.height, settings.concurrent_spp, settings.tile_size)
        , scheduler_(&scene_, settings.q_size,
                     settings.traversal_platform == UserSettings::gpu)
        , shadow_scheduler_pt_(&scene_, settings.q_size,
                               settings.traversal_platform == UserSettings::gpu)
        , shadow_scheduler_lt_(&scene_, settings.q_size,
                               settings.traversal_platform == UserSettings::gpu)
        , shadow_scheduler_connect_(&scene_, settings.q_size,
                                    settings.traversal_platform == UserSettings::gpu)
    {
        // Compute the required cache size for storing the light and camera vertices.
        bool use_gpu = settings.traversal_platform == UserSettings::gpu;
        int avg_light_v = estimate_light_path_len(scene, use_gpu, settings.light_path_count);
        int avg_cam_v = estimate_cam_path_len(scene, cam, use_gpu, 1);

        // TODO: this path length estimation is only really necessary in a GPU implementation
        //       on the CPU, we can stop wasting these rays and use them instead to build an initial distribution
        //       to guide the first iteration! The memory will be allocated on the fly for this (amortized, doubling the size every time)
        int num_cam_v   = 1.2f * avg_cam_v * settings.width * settings.height * settings.concurrent_spp;
        int num_light_v = 1.2f * avg_light_v * settings.light_path_count;

        cam_verts_.reset(new VertCache(num_cam_v));
        light_verts_.reset(new VertCache(num_light_v));
    }

    void render(AtomicImage& img) override final;

    void reset() override final {
        pm_radius_ = base_radius_;
        cur_iteration_ = 0;
    }

    void preprocess() override final {
        Integrator::preprocess();
        base_radius_ = pixel_size() * settings_.radius_factor;
    }

private:
    UserSettings settings_;

    int cur_iteration_;
    float pm_radius_;
    float base_radius_;
    float merge_pdf_;

    UniformLightTileGen<State> light_tile_gen_;
    DefaultTileGen<State> camera_tile_gen_;

    DeferredScheduler<State> scheduler_;
    DeferredScheduler<ShadowState, true> shadow_scheduler_pt_;
    DeferredScheduler<ShadowState, true> shadow_scheduler_lt_;
    DeferredScheduler<ShadowState, true> shadow_scheduler_connect_;

    std::unique_ptr<VertCache> cam_verts_;
    std::unique_ptr<VertCache> light_verts_;

    HashGrid<VertexHandle> photon_grid_;

    void trace_camera_paths();
    void trace_light_paths();
    void process_hits(Ray& r, Hit& h, State& s, VertCache* cache, bool adjoint);
    void process_envmap_hits(Ray& r, State& s);

    void bounce(State& state_out, const Intersection& isect, BSDF* bsdf, Ray& ray_out, bool adjoint, float offset);

    /// Computes the cosine term for adjoint BSDFs that use shading normals.
    ///
    /// This function has to be used for all BSDFs while tracing paths from the light sources, to prevent brighness discontinuities.
    /// See Veach's thesis for more details.
    inline static float shading_normal_adjoint(const float3& normal, const float3& geom_normal, const float3& out_dir, const float3& in_dir) {
        return dot(out_dir, normal) * dot(in_dir, geom_normal) / dot(out_dir, geom_normal);
    }

    // Sampling techniques (additional to camera rays hitting the light)
    void path_tracing(AtomicImage& img, bool next_evt);
    void light_tracing(AtomicImage& img);
    void connect(AtomicImage& img);
    void merge(AtomicImage& img);
};

} // namespace imba

#endif // IMBA_DEFERRED_VCM_H