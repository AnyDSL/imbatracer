#ifndef IMBA_LIGHT_VERTICES_H
#define IMBA_LIGHT_VERTICES_H

#include "integrator.h"

#include "../../rangesearch/rangesearch.h"
#include "../../core/float4.h"
#include "../../core/common.h"

#include "../random.h"

#define NOMINMAX
#include <tbb/tbb.h>

namespace imba {

/// Stores the data required for connecting (or merging) a camera vertex to (with) a light vertex.
struct LightPathVertex {
    Intersection isect;
    rgb throughput;

    int path_length;

    // partial weights for MIS, see VCM technical report
    float dVC;
    float dVCM;
    float dVM;

    LightPathVertex(Intersection isect, rgb tp, float dVC, float dVCM, float dVM, int path_length)
        : isect(isect), throughput(tp), dVC(dVC), dVCM(dVCM), dVM(dVM), path_length(path_length)
    {}

    LightPathVertex() {}

    float3& position() { return isect.pos; }
    const float3& position() const { return isect.pos; }
};

using PhotonIterator = std::vector<LightPathVertex>::iterator;

struct VCMPhoton {
    float3 position;
    float3 out_dir;
    float dVCM;
    float dVM;
    rgb throughput;

    VCMPhoton() {}
    VCMPhoton(const LightPathVertex& r) {
        position   = r.isect.pos;
        out_dir    = r.isect.out_dir;
        dVCM       = r.dVCM;
        dVM        = r.dVM;
        throughput = r.throughput;
    }
};

/// Stores the vertices of the light paths and implements selecting vertices for connecting and merging.
class LightVertices {
    // Number of light paths to be traced when computing the average length and thus vertex cache size.
    static const int LIGHT_PATH_LEN_PROBES = 10000;
public:
    LightVertices(int path_count, int spp)
        : vertex_caches_(spp)
        , vertex_cache_last_(spp)
        , light_vertices_count_(spp)
        , photon_grid_(spp)
        , path_count_(path_count)
        , spp_(spp)
    {}

    void compute_cache_size(const Scene& scene, bool use_gpu = true);

    void build(float radius, bool use_merging) {
        tbb::parallel_for(tbb::blocked_range<int>(0, spp_), [&] (const tbb::blocked_range<int>& range) {
            for (auto i = range.begin(); i != range.end(); ++i) {
                light_vertices_count_[i] = std::min(static_cast<int>(vertex_caches_[i].size()), static_cast<int>(vertex_cache_last_[i]));
                if (use_merging) {
                    photon_grid_[i].build(vertex_caches_[i].begin(), vertex_caches_[i].begin() + light_vertices_count_[i], radius);
                }
            }
        });
    }

    inline void add_vertex_to_cache(const LightPathVertex& v, int sample_id) {
        int i = vertex_cache_last_[sample_id]++;
        if (i > vertex_caches_[sample_id].size())
            return; // Discard vertices that do not fit. This is very unlikely to happen.
        vertex_caches_[sample_id][i] = v;
    }

    inline int count(int sample_id) {
        return light_vertices_count_[sample_id];
    }

    /// Returns a random vertex that can be used to connect to (BPT)
    inline LightPathVertex& get_connect(int sample_id, RNG& rng) {
        return vertex_caches_[sample_id][rng.random_int(0, light_vertices_count_[sample_id])];
    }

    /// Fills the given container with all photons within the radius around the given point.
    template<typename Container>
    inline void get_merge(int sample_id, const float3& pos, Container& out) {
        photon_grid_[sample_id].process(out, pos);
    }

    void clear() {
        for(auto& i : vertex_cache_last_)
            i = 0;
    }

private:
    // Light path vertices and associated data are stored separately per sample / iteration.
    std::vector<std::vector<LightPathVertex> > vertex_caches_;
    std::vector<std::atomic<int> > vertex_cache_last_;
    std::vector<int> light_vertices_count_;
    std::vector<HashGrid<PhotonIterator, VCMPhoton> > photon_grid_;

    int path_count_;
    int spp_;
};

} // namespace imba

#endif
