#ifndef IMBA_DEFERRED_VERTICES_H
#define IMBA_DEFERRED_VERTICES_H

#include "imbatracer/render/scene.h"
#include "imbatracer/render/ray_gen/camera.h"

#include <vector>
#include <atomic>
#include <iostream>

namespace imba {

/// Traces a number of light paths through the scene and computes their average length.
int estimate_light_path_len(const Scene& scene, bool use_gpu, int probes);

/// Traces a number of camera paths through the scene and computes their average length.
int estimate_cam_path_len(const Scene& scene, const PerspectiveCamera& cam, bool use_gpu, int probes);

template <typename Vertex>
class DeferredVertices {
public:
    DeferredVertices(int capacity) : verts_(capacity), next_(0) {}

    /// Adds a vertex to the cache.
    /// \returns The index of the vertex
    int add(const Vertex& v) {
        int i = next_++;
        if (i >= verts_.size()) {
            std::cout << "A vertex did not fit into the cache!" << std::endl;
            return -1;
        } else
            verts_[i] = v;
        return i;
    }

    int size() const { return std::min(static_cast<int>(verts_.size()), next_.load()); }
    const Vertex& operator[] (int i) const { assert(i < size()); return verts_[i]; }

    void clear() { next_.store(0); }

private:
    std::vector<Vertex> verts_;
    std::atomic<int> next_;
};

} // namespace imba

#endif // IMBA_DEFERRED_VERTICES_H