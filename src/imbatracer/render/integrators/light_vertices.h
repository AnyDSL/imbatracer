#ifndef IMBA_LIGHT_VERTICES_H
#define IMBA_LIGHT_VERTICES_H

#include "integrator.h"

#include "../../rangesearch/rangesearch.h"
#include "../../rangesearch/rangesearch_impala_interface.h"
#include "../../core/float4.h"
#include "../../core/common.h"

#include "../random.h"

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

/// Wrapper of std::vector<LightPathVertex> to addtionally store LightPathVertex position in a contigous fashion
struct VertexCache {
    std::vector<LightPathVertex> vertex_cache;
    thorin::Array<float> vertex_poses; // this array is redundant, but serves well for photon mapping query
   
    // add wrapper methods at here to avoid changing code at somewhere else
    size_t size() { return vertex_cache.size(); }
    void resize(const int &s) { vertex_cache.resize(s); vertex_poses = std::move(thorin::Array<float>(3 * s)); }
    PhotonIterator begin() { return vertex_cache.begin(); }
    LightPathVertex& operator[](size_t pos) { return vertex_cache[pos]; }
    const LightPathVertex& operator[](size_t pos) const { return vertex_cache[pos]; }
};

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

    void compute_cache_size(const Scene& scene);

    void build(float radius, bool use_merging) {
        tbb::parallel_for(tbb::blocked_range<int>(0, spp_), [&] (const tbb::blocked_range<int>& range) {
            for (auto i = range.begin(); i != range.end(); ++i) {
                light_vertices_count_[i] = std::min(static_cast<int>(vertex_caches_[i].size()), static_cast<int>(vertex_cache_last_[i]));
                if (use_merging) {
                    photon_grid_[i].build(vertex_caches_[i].vertex_poses, light_vertices_count_[i], radius);
                }
            }
        });
    }

    inline void add_vertex_to_cache(const LightPathVertex& v, int sample_id) {
        int i = vertex_cache_last_[sample_id]++;
        if (i > vertex_caches_[sample_id].size())
            return; // Discard vertices that do not fit. This is very unlikely to happen.
        vertex_caches_[sample_id][i] = v;
        // 
        const float3& pos = v.position(); 
        float *p = &(vertex_caches_[sample_id].vertex_poses[3 * i]);
        p[0] = pos.x;
        p[1] = pos.y;
        p[2] = pos.z;
    }

    inline int count(int sample_id) {
        return light_vertices_count_[sample_id];
    }

    /// Returns a random vertex that can be used to connect to (BPT)
    inline LightPathVertex& get_connect(int sample_id, RNG& rng) {
        return vertex_caches_[sample_id][rng.random_int(0, light_vertices_count_[sample_id])];
    }

    /// Calls Impala API to get batch query result returned as a struct 
    inline BatchQueryResult* get_merge(int sample_id, thorin::Array<float> &host_poses, int size) {
        return photon_grid_[sample_id].process(host_poses, size);
    }

    inline PhotonIterator begin(int sample_id) {
        return vertex_caches_[sample_id].begin();
    }

    void clear() {
        for(auto& i : vertex_cache_last_)
            i = 0;
    }

    int get_query_time_count(int sample_id) {
        return photon_grid_[sample_id].get_query_time_count(); }

    int get_query_copy_time(int sample_id) {
        return photon_grid_[sample_id].get_query_copy_time(); }

private:
    // Light path vertices and associated data are stored separately per sample / iteration.
    std::vector<VertexCache> vertex_caches_;
    std::vector<std::atomic<int> > vertex_cache_last_;
    std::vector<int> light_vertices_count_;
    std::vector<HashGrid<PhotonIterator, VCMPhoton> > photon_grid_;

    int path_count_;
    int spp_;
};

} // namespace imba

#endif
