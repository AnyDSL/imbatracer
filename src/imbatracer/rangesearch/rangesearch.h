#ifndef IMBA_RANGESEARCH_H
#define IMBA_RANGESEARCH_H

#include "../core/float4.h"
#include "rangesearch_impala_interface.h"
#include <thorin_runtime.hpp>

#include <vector>

namespace imba {

struct CellIdx {
    int x;
    int y;

    CellIdx(int x, int y) : x(x), y(y) {}
    CellIdx() : x(0), y(0) {}
};

/// Prelim. photon range search accelerator, taken from SmallVCM
/// Will be replaced later on by a high performance parallel version.
template<typename Iter>
class HashGrid {
public:
    void reserve(int num_cells) {
        cell_ends_.resize(num_cells);
    }

    void build(const Iter& photons_begin, const Iter& photons_end, float radius) {
        radius_        = radius;
        radius_sqr_    = sqr(radius_);
        cell_size_     = radius_ * 2.f; // TODO Is it the best?
        inv_cell_size_ = 1.f / cell_size_;

        bbox_min_ = float3( 1e36f);
        bbox_max_ = float3(-1e36f);

        int photon_count = 0;
        for(Iter it = photons_begin; it != photons_end; ++it) { // TODO How to paralleliz? concurrent access for bbox_max_(rw) and bbox_min_(rw) and photon_count(w)
            const float3 &pos = it->position();
            for(int j=0; j<3; j++) {
                bbox_max_[j] = std::max(bbox_max_[j], pos[j]);
                bbox_min_[j] = std::min(bbox_min_[j], pos[j]);
            }
            ++photon_count;
        }

        indices_.resize(photon_count);
        memset(&cell_ends_[0], 0, cell_ends_.size() * sizeof(int));

        // set cell_ends_[x] to number of particles within x
        for(Iter it = photons_begin; it != photons_end; ++it) { // TODO How to parallelize? cell_ends_(w)
            const float3 &pos = it->position();
            cell_ends_[cell_index(pos)]++;
        }

        // run exclusive prefix sum to really get the cell starts
        // cell_ends_[x] is now where the cell starts
        // TODO can combine next two for loop into one, get ends directly and use --x in the second for loop instead of x++
        int sum = 0;
        for(size_t i = 0; i < cell_ends_.size(); i++) { // TODO How to parallelize?
            int temp = cell_ends_[i];
            cell_ends_[i] = sum;
            sum += temp;
        }

        for(Iter it = photons_begin; it != photons_end; ++it) { // TODO How to parallelize?
            const float3 &pos = it->position();
            const int target_idx = cell_ends_[cell_index(pos)]++;
            indices_[target_idx] = it;
        }

        // init data used in impala
        initImpalaData();
    }

    template<typename Container>
    void process(Container& output, const float3& query_pos) {
///*
        // Impala Impl: Init data
        struct QueryResult *result = nullptr;
	struct Float3 pos;

	pos.x = query_pos.x;
        pos.y = query_pos.y;
        pos.z = query_pos.z;

        //thorin::Array<int> cell_ends_array(cell_ends_.size());
        //std::copy(cell_ends_.begin(), cell_ends_.end(), cell_ends_array.begin());
        
        // Impala Impl: Call Interface
        hashgrid_query(result, &data_, pos);
 
        // Impala Impl: Postprocess
        for (int i = 0; i < result->size; ++i) {
            Iter it = *(reinterpret_cast<Iter*>(result->pointers[i]));
            // radius filter
            // TODO how to put it into impala code
            // define a function in c++ as interface to compute and return distSqr
            const float distSqr = lensqr(query_pos - it->position());
            if (distSqr <= radius_sqr_)
                output.push_back(it);
        }


        // Impala Impl: Free memory
        if (result) {
            if (result->pointers)
                delete [] result->pointers;
            delete result;
        }
//*/


/*
        const float3 dist_min = query_pos - bbox_min_;
        const float3 dist_max = bbox_max_ - query_pos;

        // Check if the position is outside the bounding box.
        for(int i = 0; i < 3; i++) {
            if(dist_min[i] < 0.f) return;
            if(dist_max[i] < 0.f) return;
        }

        const float3 cell = inv_cell_size_ * dist_min;
        const float3 coord(
            std::floor(cell.x),
            std::floor(cell.y),
            std::floor(cell.z));

        const int  px = int(coord.x);
        const int  py = int(coord.y);
        const int  pz = int(coord.z);

        const float3 fract_coord = cell - coord;

        const int  pxo = px + (fract_coord.x < 0.5f ? -1 : 1);
        const int  pyo = py + (fract_coord.y < 0.5f ? -1 : 1);
        const int  pzo = pz + (fract_coord.z < 0.5f ? -1 : 1);

        for(int j = 0; j < 8; j++) { // TODO Is it possible same photon get pushed back more than only once? i.e. a collision in hash fn
            CellIdx active_range;
            auto z = j & 1 ? pzo : pz;
            auto y = j & 2 ? pyo : py;
            auto x = j & 4 ? pxo : px;
            active_range = cell_range(cell_index(x, y, z));

            for(; active_range.x < active_range.y; active_range.x++) // TODO How to parallelize?
            {
                const Iter particle_idx = indices_[active_range.x];
                const float distSqr = lensqr(query_pos - particle_idx->position());

                if(distSqr <= radius_sqr_)
                    output.push_back(particle_idx);
            }
        }
*/

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
        const float3 dist_min = point - bbox_min_;

        const float3 coord(
            std::floor(inv_cell_size_ * dist_min.x),
            std::floor(inv_cell_size_ * dist_min.y),
            std::floor(inv_cell_size_ * dist_min.z));

        return cell_index(int(coord.x), int(coord.y), int(coord.z));
    }

    void initImpalaData() {
	// init iter_pointers_
	size_t size = indices_.size();
        iter_pointers_.resize(size);
        for (size_t i = 0; i < size; ++i) {
            iter_pointers_[i] = reinterpret_cast<unsigned long long>(&indices_[i]);
        }
	
	// init data_
        data_.bbox_min.x = bbox_min_.x;
        data_.bbox_min.y = bbox_min_.y;
        data_.bbox_min.z = bbox_min_.z;

        data_.bbox_max.x = bbox_max_.x;
        data_.bbox_max.y = bbox_max_.y;
        data_.bbox_max.z = bbox_max_.z;

        data_.radius = radius_;
        data_.radius_sqr = radius_sqr_;
        data_.cell_size = cell_size_;
        data_.inv_cell_size = inv_cell_size_;
        data_.indices_size = (int)indices_.size();
        data_.cell_ends_size = (int)cell_ends_.size();
        data_.indices = iter_pointers_.data();
        data_.cell_ends = cell_ends_.data();   
    }

    float3 bbox_min_;
    float3 bbox_max_;
    std::vector<Iter> indices_;
    std::vector<int> cell_ends_;

    float radius_;
    float radius_sqr_;
    float cell_size_;
    float inv_cell_size_;

    // Impala data
    std::vector<unsigned long long> iter_pointers_; // holds the address of the Iter, serves as another representation as indices_
    struct HashGridInfo data_;
    
};

}

#endif
