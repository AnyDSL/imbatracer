#ifndef IMBA_RANGESEARCH_H
#define IMBA_RANGESEARCH_H

#include "../core/float4.h"

#include <vector>

#define NOMINMAX
#include <tbb/tbb.h>

namespace imba {

struct CellIdx {
    int x;
    int y;

    CellIdx(int x, int y) : x(x), y(y) {}
    CellIdx() : x(0), y(0) {}
};

typedef unsigned int uint;

template<typename Iter, typename Photon>
class HashGrid {
public:
    HashGrid() {}

    void build(const Iter& photons_begin, const Iter& photons_end, float radius) {
        constexpr int inv_load_factor = 2;
        radius_        = radius;
        radius_sqr_    = sqr(radius_);
        cell_size_     = radius_ * 2.f;
        inv_cell_size_ = 1.f / cell_size_;

        int photon_count = photons_end - photons_begin;
        if (cell_ends_.size() < photon_count * inv_load_factor)
            cell_ends_ = std::vector<std::atomic<int>>(photon_count * inv_load_factor);

        // Compute the extents of the bounding box.
        bbox_ = tbb::parallel_reduce(tbb::blocked_range<Iter>(photons_begin, photons_end), BBox::empty(),
            [] (const tbb::blocked_range<Iter>& range, BBox init) {
                for (Iter it = range.begin(); it != range.end(); ++it) init.extend(it->position());
                return init;
            },
            [] (BBox a, const BBox& b) { return a.extend(b); });

        // Distribute the photons to the HashGrid cells using Counting Sort.
        photons_.resize(photon_count);
        std::fill(cell_ends_.begin(), cell_ends_.end(), 0);

        // Count the number of photons in each cell.
        tbb::parallel_for(tbb::blocked_range<Iter>(photons_begin, photons_end), [this](const tbb::blocked_range<Iter>& range){
            for (Iter it = range.begin(); it != range.end(); ++it) {
                cell_ends_[cell_index(it->position())]++;
            }
        });

        // Set the cell_ends_[x] to the first index that belongs to the respective cell.
        int sum = 0;
        for(size_t i = 0; i < cell_ends_.size(); i++) {
            int temp = cell_ends_[i];
            cell_ends_[i] = sum;
            sum += temp;
        }

        // Assign the photons to the cells.
        tbb::parallel_for(tbb::blocked_range<Iter>(photons_begin, photons_end), [this](const tbb::blocked_range<Iter>& range){
            for (Iter it = range.begin(); it != range.end(); ++it) {
                const float3 &pos = it->position();
                const int target_idx = cell_ends_[cell_index(pos)]++;
                photons_[target_idx] = *it;
            }
        });
    }

    template <typename ResultFn>
    void process(const float3& query_pos, ResultFn result) {
        // Check if the position is outside the bounding box.
        if (!bbox_.is_inside(query_pos)) return;

        const float3 cell = inv_cell_size_ * (query_pos - bbox_.min);
        const float3 coord(
            std::floor(cell.x),
            std::floor(cell.y),
            std::floor(cell.z));

        const int px = int(coord.x);
        const int py = int(coord.y);
        const int pz = int(coord.z);

        const float3 fract_coord = cell - coord;

        const int pxo = px + (fract_coord.x < 0.5f ? -1 : 1);
        const int pyo = py + (fract_coord.y < 0.5f ? -1 : 1);
        const int pzo = pz + (fract_coord.z < 0.5f ? -1 : 1);

        for (int j = 0; j < 8; j++) {
            const int x = j & 4 ? pxo : px;
            const int y = j & 2 ? pyo : py;
            const int z = j & 1 ? pzo : pz;
            CellIdx active_range = cell_range(cell_index(x , y , z ));

            for (; active_range.x < active_range.y; active_range.x++) {
                const auto& photon = photons_[active_range.x];
                const float dist_sqr = lensqr(query_pos - photon.position);

                if (dist_sqr <= radius_sqr_)
                    result(dist_sqr, photon);
            }
        }
    }

private:
    CellIdx cell_range(int cell_idx) const {
        if(cell_idx == 0) return CellIdx(0, cell_ends_[0]);
        return CellIdx(cell_ends_[cell_idx-1], cell_ends_[cell_idx]);
    }

    int cell_index(uint x, uint y, uint z) const {
        return int(((x * 73856093) ^ (y * 19349663) ^
            (z * 83492791)) % uint(cell_ends_.size()));
    }

    int cell_index(const float3 &point) const {
        const float3 dist_min = inv_cell_size_ * (point - bbox_.min);
        int coord_x = std::floor(dist_min.x);
        int coord_y = std::floor(dist_min.y);
        int coord_z = std::floor(dist_min.z);
        return cell_index(coord_x, coord_y, coord_z);
    }

    BBox bbox_;
    std::vector<Photon> photons_;
    std::vector<std::atomic<int>> cell_ends_;

    float radius_;
    float radius_sqr_;
    float cell_size_;
    float inv_cell_size_;
};

}

#endif
