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
    LightVertices(int path_count)
        : path_count_(path_count)
    {}

    void compute_cache_size(const Scene& scene, bool use_gpu);

    /// Builds the acceleration structure etc to prepare the cache for usage during rendering
    void build(float radius, bool use_merging) {
        count_ = std::min<int>(cache_.size(), last_.load());
        if (use_merging) {
            accel_.build(cache_.begin(), cache_.begin() + count_, radius);
        }
    }

    inline void add_vertex_to_cache(const LightPathVertex& v) {
        int i = last_++;
        if (i > cache_.size())
            return; // Discard vertices that do not fit. This is very unlikely to happen.
        cache_[i] = v;
    }

    inline int count() const {
        return count_;
    }

    /// Returns a random vertex that can be used to connect to (BPT)
    inline const LightPathVertex& get_connect(RNG& rng) const {
        return cache_[rng.random_int(0, count_)];
    }

    /// Fills the given container with all photons within the radius around the given point.
    template <typename Container>
    inline void get_merge(const float3& pos, Container& out) const {
        accel_.query(pos, out);
    }

    /// Removes all vertices currently inside the cache
    void clear() {
        last_.store(0);
    }

private:
    /// Stores all light vertices, without any path structure
    std::vector<LightPathVertex> cache_;

    /// Index of the last free element in the vertex cache
    std::atomic<int> last_;

    /// Number of light vertices currently in the cache,
    /// separated from last_ because overflow is ignored
    int count_;

    /// Acceleration structure for photon range queries
    HashGrid<PhotonIterator, VCMPhoton> accel_;

    /// Number of light paths that will be traced and stored in this cache
    int path_count_;
};

} // namespace imba

#endif
