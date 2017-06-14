#ifndef IMBA_LIGHT_VERTICES_H
#define IMBA_LIGHT_VERTICES_H

#include "imbatracer/render/integrators/integrator.h"

#include "imbatracer/rangesearch/rangesearch.h"
#include "imbatracer/core/float4.h"
#include "imbatracer/core/common.h"

#include "imbatracer/render/random.h"

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

    atomic_rgb total_contrib_pm;
    atomic_rgb total_contrib_vc;

    LightPathVertex(Intersection isect, rgb tp, float dVC, float dVCM, float dVM, int path_length)
        : isect(isect), throughput(tp), dVC(dVC), dVCM(dVCM), dVM(dVM), path_length(path_length)
    {
        total_contrib_pm = rgb(0.0f);
    }

    LightPathVertex() {}

    float3& position() { return isect.pos; }
    const float3& position() const { return isect.pos; }

    LightPathVertex& operator= (const LightPathVertex& r) {
        isect       = r.isect;
        throughput  = r.throughput;
        path_length = r.path_length;
        dVC         = r.dVC;
        dVCM        = r.dVCM;
        dVM         = r.dVM;

        total_contrib_pm = rgb(0.0f);
        total_contrib_vc = rgb(0.0f);

        return *this;
    }

    LightPathVertex(const LightPathVertex& r) {
        isect       = r.isect;
        throughput  = r.throughput;
        path_length = r.path_length;
        dVC         = r.dVC;
        dVCM        = r.dVCM;
        dVM         = r.dVM;

        total_contrib_pm = rgb(0.0f);
        total_contrib_vc = rgb(0.0f);
    }
};

using PhotonIterator = std::vector<LightPathVertex>::iterator;

struct VCMPhoton {
    float3 pos;
    float3 out_dir;
    float dVCM;
    float dVM;
    rgb throughput;

    LightPathVertex* vert;

    const float3& position() const { return pos; }

    VCMPhoton() {}
    VCMPhoton(LightPathVertex& r) {
        pos        = r.isect.pos;
        out_dir    = r.isect.out_dir;
        dVCM       = r.dVCM;
        dVM        = r.dVM;
        throughput = r.throughput;

        vert = &r;
    }
};

/// Stores the vertices of the light paths and implements selecting vertices for connecting and merging.
class LightVertices {
    // Number of light paths to be traced when computing the average length and thus vertex cache size.
    static const int LIGHT_PATH_LEN_PROBES = 10000;
public:
    using VertIter = std::vector<LightPathVertex>::const_iterator;

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

    inline LightPathVertex* add_vertex_to_cache(LightPathVertex&& v) {
        int i = last_++;
        if (i >= cache_.size()) {
            std::cout << "A vertex did not fit into the cache!" << std::endl;
            return nullptr; // Discard vertices that do not fit. This is very unlikely to happen.
        }
        return &(cache_[i] = v);
    }

    inline int count() const {
        return count_;
    }

    /// Returns a random vertex that can be used to connect to (BPT)
    inline LightPathVertex& get_connect(RNG& rng) {
        return cache_[rng.random_int(0, count_)];
    }

    /// Fills the given container with all photons within the radius around the given point.
    /// \returns The number of photons found
    template <typename Container>
    inline int get_merge(const float3& pos, Container& out, int k) {
        return accel_.query(pos, out, k);
    }

    /// Removes all vertices currently inside the cache
    void clear() {
        last_.store(0);
    }

    VertIter begin() const {
        return cache_.begin();
    }

    VertIter end() const {
        return cache_.begin() + count_;
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
    HashGrid<VCMPhoton> accel_;

    /// Number of light paths that will be traced and stored in this cache
    int path_count_;
};

} // namespace imba

#endif
