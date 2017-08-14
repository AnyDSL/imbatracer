#ifndef IMBA_DEFERRED_VCM_H
#define IMBA_DEFERRED_VCM_H

#include "imbatracer/render/materials/material_system.h"
#include "imbatracer/render/scheduling/deferred_scheduler.h"
#include "imbatracer/render/scheduling/queue_scheduler.h"

#include "imbatracer/render/integrators/integrator.h"
#include "imbatracer/render/integrators/deferred_vertices.h"
#include "imbatracer/render/integrators/deferred_mis.h"

#include "imbatracer/render/debug/path_debug.h"

#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/ray_gen/ray_gen.h"

#include "imbatracer/rangesearch/rangesearch.h"

#include "imbatracer/frontend/cmd_line.h"

// Use this to enable writing paths to file. Can be used to visualize the most interesting
// light paths, or for debugging fireflies.
// #define PATH_STATISTICS

namespace imba {

template <typename MisType>
class DeferredVCM : public Integrator {
    struct State : public RayState {
        union {
            int pixel_id;
            int light_id;
        };

        /// Random number generator used to sample all rays along this path.
        RNG rng;

        /// The power or importance carried by the path up to this intersection
        rgb throughput;

        /// Number of vertices along this path until now (includes vertex at camera / light)
        uint16_t path_length;

        /// Index within the vertex cache where the previous vertex along this path was stored
        /// -1 if the ancestor was not stored (e.g. is on a specular surface, first vertex, cache too small, ...)
        int32_t ancestor;

        bool adjoint;

        MisType mis;
    };

    struct ShadowState {
        /// Weighted contribution of the shadow ray if it is not occluded
        rgb contrib;

        int pixel_id;
    };

    struct LocalStats {
        rgb contrib;
        float mis_weight;
    };

    struct Vertex {
        State state;

        Intersection isect;
        bool specular;

        LocalStats density;

        Vertex() {}
        Vertex(State state, const Intersection& isect, bool specular)
            : state(state), isect(isect), specular(specular)
        {}
    };

    struct ShadowStateConnectDbg {
        /// Weighted contribution of the shadow ray if it is not occluded
        rgb contrib;

        int pixel_id;

#ifdef PATH_STATISTICS
        const Vertex* cam;
        const Vertex* light;
        float mis_weight;
#endif
    };

    struct VertexHandle {
        VertexHandle() : vert(nullptr) {}
        VertexHandle(const Vertex& v) { vert = &v; }
        VertexHandle(const Vertex* v) { vert = v; }
        VertexHandle& operator= (const Vertex& v) { vert = &v; return *this; }
        VertexHandle& operator= (const VertexHandle* v) { vert = v->vert; return *this; }

        const float3& position() const { return vert->isect.pos; }

        const Vertex* vert;
    };

    using VertCache = DeferredVertices<Vertex>;

public:
    DeferredVCM(Scene& scene, PerspectiveCamera& cam, const UserSettings& settings)
        : Integrator(scene, cam)
        , settings_(settings)
        , cur_iteration_(0)
        // TODO configure settings
        , scheduler_(512, 256 * 256, 4, settings.traversal_platform == UserSettings::gpu)
        , shadow_scheduler_pt_(512, 256 * 256, 4, settings.traversal_platform == UserSettings::gpu)
        , shadow_scheduler_lt_(512, 256 * 256, 4, settings.traversal_platform == UserSettings::gpu)
        , shadow_scheduler_connect_(512, 256 * 256, 4, settings.traversal_platform == UserSettings::gpu)
    {
        cam_verts_  [0].reset(new VertCache());
        light_verts_[0].reset(new VertCache());
        cam_verts_  [1].reset(new VertCache());
        light_verts_[1].reset(new VertCache());
    }

    void render(AtomicImage& img) override final;

    void reset() override final {
        pm_radius_ = base_radius_;
        cur_iteration_ = 0;
        cur_cache_ = 0;
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

    QueueScheduler<State> scheduler_;
    QueueScheduler<ShadowState> shadow_scheduler_pt_;
    QueueScheduler<ShadowState> shadow_scheduler_lt_;
    QueueScheduler<ShadowState> shadow_scheduler_connect_;

    std::unique_ptr<VertCache> cam_verts_[2];
    std::unique_ptr<VertCache> light_verts_[2];
    int cur_cache_;

    HashGrid<VertexHandle> photon_grid_;
    HashGrid<VertexHandle> importon_grid_;

    PathDebugger<Vertex> path_log_;

    void trace();
    void trace_camera_primary();
    void trace_light_primary();
    void process_hits(Ray& r, Hit& h, State& s);
    void process_envmap_hits(Ray& r, State& s);
    void bounce(State& state_out, const Intersection& isect, BSDF* bsdf, Ray& ray_out, bool adjoint, float offset, float rr_pdf);

    /// Computes the contribution of all paths in \see{from} after \see{offset} via density estimation with the vertices in \see{accel}
    /// The resulting unweighted contribution is stored directly in the vertices, along with their MIS weights.
    /// \param num_paths The number of paths traced to generate the vertices in \see{accel}
    void compute_local_stats(VertCache& from, int offset, const HashGrid<VertexHandle>& accel, int num_paths);

    // Sampling techniques (additional to camera rays hitting the light)
    void path_tracing (AtomicImage& img, bool next_evt);
    void light_tracing(AtomicImage& img);
    void connect      (AtomicImage& img);
    void merge        (AtomicImage& img);

    void begin_iteration();
    void end_iteration();
};

} // namespace imba

#endif // IMBA_DEFERRED_VCM_H