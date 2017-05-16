#ifndef IMBA_CONTRIB_GRID_H
#define IMBA_CONTRIB_GRID_H

#include "imbatracer/core/bbox.h"

#define NOMINMAX
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

    /// Builds the grid from the given vertices.
    /// \param begin    Forward iterator, first vertex
    /// \param end      Forward iterator, behind last vertex
    /// \param contrib  Function that computes the contribution of a vertex
    /// \param pos      Function that computes the position of a vertex
    template <typename ForIter>
    void build(ForIter begin, ForIter end, ContribFn contrib, PosFn pos, int nx, int ny, int nz, const BBox& bounds) {
        nx_ = nx; ny_ = ny; nz_ = nz;
        grid_ = GridContainer(nx * ny * nz);

        for (int i = 0; i < grid_.size(); ++i)
            for (int k = 0; k < N; ++k) grid_[i][k] = 0.0f;

        // Add a safety margin to the bounding box.
        bbox_ = bounds;
        auto extents = bbox_.max - bbox_.min;
        bbox_.max += extents * 0.01f;
        bbox_.min -= extents * 0.01f;
        extents = bbox_.max - bbox_.min;

        inv_cell_size_ = float3(nx / extents.x,
                                ny / extents.y,
                                nz / extents.z);

        // Compute the total contributions
        std::array<std::atomic<float>, N> max_val;
        for (int k = 0; k < N; ++k) max_val[k] = 0.0f;
        tbb::parallel_for(tbb::blocked_range<ForIter>(begin, end),
            [this, &max_val, &pos, &contrib] (const tbb::blocked_range<ForIter>& range){
            for (ForIter i = range.begin(); i != range.end(); ++i) {
                int idx = cell_index(pos(*i));
                assert(idx < grid_.size());

                float c[N];
                contrib(*i, c);
                for (int k = 0; k < N; ++k) {
                    atomic_add(grid_[idx][k], c[k]);

                    // Compute the maximum value in a grid cell on-the-fly.
                    // Assumes that all contributions are greater than zero!
                    float v = grid_[idx][k];
                    atomic_max(max_val[k], v);

                    assert(c[k] > 0.0f);
                }
            }
        });

        // Normalize
        tbb::parallel_for(tbb::blocked_range<int>(0, grid_.size()),
            [this, &max_val, &pos, &contrib] (const tbb::blocked_range<int>& range){
            for (int i = range.begin(); i != range.end(); ++i) {
                for (int k = 0; k < N; ++k) {
                    float v = grid_[i][k];
                    grid_[i][k] = v / max_val[k];
                }
            }
        });
    }

    float operator()(const float3& p, int i) {
        return grid_[cell_index(p)][i];
    }

private:
    GridContainer grid_;
    int nx_, ny_, nz_;
    BBox bbox_;
    float3 inv_cell_size_;

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