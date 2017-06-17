#ifndef IMBA_CONTRIB_GRID_H
#define IMBA_CONTRIB_GRID_H

#include "imbatracer/core/bbox.h"

#define NOMINMAX
#define TBB_USE_EXCEPTIONS 0
#include <tbb/tbb.h>

#include <functional>

namespace imba {

template <typename T>
static T atomic_add(std::atomic<T>& a, T b) {
    T old_val = a.load();
    T desired_val = old_val + b;
    while(!a.compare_exchange_weak(old_val, desired_val))
        desired_val = old_val + b;
    return desired_val;
}

template <typename T>
static void atomic_max(std::atomic<T>& a, T b) {
    T old_val = a.load();
    T desired_val = b;
    while(b > old_val && !a.compare_exchange_weak(old_val, desired_val))
        old_val = a.load();
}

/// A regular grid that stores the total contribution (power and/or importance) of a set of vertices (photons or importons).
template <typename VertexT, int N>
class ContribGrid {
    using GridContainer = std::vector<std::array<std::atomic<float>, N>>;
public:
    typedef std::function<void (VertexT&, float[N])> ContribFn;
    typedef std::function<float3(VertexT&)> PosFn;

    ContribGrid() {}

    ContribGrid(int nx, int ny, int nz, const BBox& bounds, float v = 0.0f) {
        init(nx, ny, nz, bounds, v);
    }

    void init(int nx, int ny, int nz, const BBox& bounds, float v) {
        nx_ = nx; ny_ = ny; nz_ = nz;
        grid_ = GridContainer(nx * ny * nz);
        reset(v);

        // Add a safety margin to the bounding box.
        bbox_ = bounds;
        auto extents = bbox_.max - bbox_.min;
        bbox_.max += extents * 0.01f;
        bbox_.min -= extents * 0.01f;

        extents = bbox_.max - bbox_.min;
        inv_cell_size_ = float3(nx / extents.x,
                                ny / extents.y,
                                nz / extents.z);
    }

    void reset(float v = 0.0f) {
        for (int i = 0; i < grid_.size(); ++i)
            for (int k = 0; k < N; ++k) grid_[i][k] = v;
        for (int k = 0; k < N; ++k) max_val_[k] = v;
    }

    void reset(int k, float v = 0.0f) {
        for (int i = 0; i < grid_.size(); ++i)
            grid_[i][k] = v;
        max_val_[k] = v;
    }

    /// Builds the grid from the given vertices and normalizes the contribution to [0,1].
    /// \param begin    Forward iterator, first vertex
    /// \param end      Forward iterator, behind last vertex
    /// \param contrib  Function that computes the contribution of a vertex
    /// \param pos      Function that computes the position of a vertex
    template <typename ForIter>
    void build(ForIter begin, ForIter end, ContribFn contrib, PosFn pos) {
        // Sum up the contributions from all vertices
        tbb::parallel_for(tbb::blocked_range<ForIter>(begin, end),
            [this, &pos, &contrib] (const tbb::blocked_range<ForIter>& range){
            for (ForIter i = range.begin(); i != range.end(); ++i) {
                float c[N];
                contrib(*i, c);
                auto p = pos(*i);
                add(c, p);
            }
        });

        normalize();
    }

    /// Adds the contribution of a vertex to the grid. Thread-safe.
    void add(const float c[N], const float3& pos) {
        for (int k = 0; k < N; ++k) add(c[k], pos, k);
    }

    /// Adds to one component of the contribution of a vertex in the grid. Thread-safe.
    void add(float c, const float3& pos, int k = 0) {
        assert(c >= 0.0f);

        int idx = cell_index(pos);
        assert(idx < grid_.size());
        atomic_add(grid_[idx][k], c);

        // Update the maximum value in a grid cell on-the-fly.
        // Assumes that all contributions are greater than zero!
        float v = grid_[idx][k];
        atomic_max(max_val_[k], v);
    }

    /// Normalizes all the contributions to the range [0,1]. NOT thread-safe!
    void normalize() {
        tbb::parallel_for(tbb::blocked_range<int>(0, grid_.size()),
            [this] (const tbb::blocked_range<int>& range){
            for (int i = range.begin(); i != range.end(); ++i) {
                for (int k = 0; k < N; ++k) {
                    float v = grid_[i][k];
                    grid_[i][k] = v / max_val_[k];
                }
            }
        });
    }

    /// Normalizes the given contribution to the range [0,1]. NOT thread-safe!
    void normalize(int k) {
        tbb::parallel_for(tbb::blocked_range<int>(0, grid_.size()),
            [this, k] (const tbb::blocked_range<int>& range){
            for (int i = range.begin(); i != range.end(); ++i) {
                float v = grid_[i][k];
                grid_[i][k] = v / max_val_[k];
            }
        });
    }

    /// Returns the normalized contribution of the grid cell containing a given point.
    /// \param p The point
    /// \param i Index of the desired component from the contribution vector.
    float operator()(const float3& p, int i = 0) {
        return grid_[cell_index(p)][i];
    }

    /// Applys an operation to every pair of value (index a and b), stores the result in the index res.
    /// Parallelized, not thread-safe.
    template <typename Op>
    void apply(int res, int a, int b) {
        tbb::parallel_for(tbb::blocked_range<int>(0, grid_.size()),
            [this, res, a, b] (const tbb::blocked_range<int>& range){
            Op op;
            for (int i = range.begin(); i != range.end(); ++i) {
                float v1 = grid_[i][a];
                float v2 = grid_[i][b];
                grid_[i][res] = op(v1, v2);
            }
        });
    }

private:
    GridContainer grid_;
    int nx_, ny_, nz_;
    BBox bbox_;
    float3 inv_cell_size_;
    std::array<std::atomic<float>, N> max_val_;

    int cell_index(uint x, uint y, uint z) const {
        return x + y * nx_ + z * nx_ * ny_;
    }

    int cell_index(const float3 &p) const {
        const auto dist_min = p - bbox_.min;
        const int coord_x = std::floor(inv_cell_size_.x * dist_min.x);
        const int coord_y = std::floor(inv_cell_size_.y * dist_min.y);
        const int coord_z = std::floor(inv_cell_size_.z * dist_min.z);

        return cell_index(coord_x, coord_y, coord_z);
    }
};

} // namespace imba

#endif // IMBA_CONTRIB_GRID_H