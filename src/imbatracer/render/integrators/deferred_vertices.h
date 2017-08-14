#ifndef IMBA_DEFERRED_VERTICES_H
#define IMBA_DEFERRED_VERTICES_H

#include "imbatracer/render/scene.h"
#include "imbatracer/render/ray_gen/camera.h"

#include <vector>
#include <atomic>
#include <iostream>

namespace imba {

template <typename Vertex>
class DeferredVertices {
public:
    DeferredVertices(int capacity = 0) : verts_(capacity), next_(0) {}

    /// Adds a vertex to the cache.
    /// \returns The index of the vertex
    int add(const Vertex& v) {
        int i = next_++;
        if (i >= verts_.size()) {
            std::cout << "A vertex did not fit into the cache! (capacity:" << verts_.size() << ")" << std::endl;
            return -1;
        } else
            verts_[i] = v;
        return i;
    }

    int size() const { return std::min(static_cast<int>(verts_.size()), next_.load()); }
    int capacity() const { return verts_.size(); }

    /// Grows the size of the cache to at least the given capacity.
    void grow(int sz) {
        if (sz < capacity()) return;

        verts_.resize(2 * sz);
    }

    const Vertex& operator[] (int i) const { assert(i < size()); return verts_[i]; }
    Vertex& operator[] (int i) { assert(i < size()); return verts_[i]; }

    void clear() { next_.store(0); }

    using iterator = typename std::vector<Vertex>::iterator;
    iterator begin() { return verts_.begin(); }
    iterator end()   { return verts_.begin() + size(); }

private:
    std::vector<Vertex> verts_;
    std::atomic<int> next_;
};

} // namespace imba

#endif // IMBA_DEFERRED_VERTICES_H