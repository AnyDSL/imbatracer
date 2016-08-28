#ifndef IMBA_RANGESEARCH_H
#define IMBA_RANGESEARCH_H

#include "../core/float4.h"
#include "rangesearch_impala_interface.h"
#include "../render/ray_queue.h"
#include <thorin_runtime.hpp>

#include <vector>
#include <iterator>
#include <tbb/tbb.h>

namespace imba {

extern std::mutex traversal_mutex;

/// Impala PhotonHashGrid Wrapper
/// Prelim. photon range search accelerator, taken from SmallVCM
/// Will be replaced later on by a high performance parallel version.
template<typename Iter, typename Photon>
class HashGrid {
public:
    ~HashGrid() {
        if (hg) {
            destroy_hashgrid(hg);
            delete hg;
            hg = nullptr;
        }
    }

    HashGrid() : hg(nullptr) {}

    void print_query_time_count() { if (hg) std::cout << "Time to query hash grid: " << hg->time_count1 << "/" << hg->time_count2 << "->" << hg->time_count1 + hg->time_count2 << std::endl; }
    
    void build(const Iter& photons_begin, const Iter& photons_end, float radius) {
        std::lock_guard<std::mutex> lock(traversal_mutex);
      	
        // memory allocation and copy onto proper device if necessary
        int photon_count = std::distance(photons_begin, photons_end);	
        thorin::Array<float> host_poses(3 * photon_count);
        for(int i = 0; i < photon_count; ++i) {
            Iter it = std::next(photons_begin, i);
            float *pos = &(host_poses.data()[3 * i]);
            const float3 &p = it->position();
            pos[0] = p[0];
            pos[1] = p[1];
            pos[2] = p[2];
        }

        photon_poses = std::move(thorin::Array<float>(thorin::Platform::RANGESEARCH_PLATFORM, thorin::Device(RANGESEARCH_DEVICE), 3 * photon_count));

        thorin::copy(host_poses, photon_poses);

        // impala API
	    hg = build_hashgrid(hg, photon_poses.data(), photon_count, CELL_ENDS_SIZE, radius);
    }


    BatchQueryResult* process(float3* query_poses, const int size) {
        std::lock_guard<std::mutex> lock(traversal_mutex);
        
        // memory allocation and copy onto proper device if necessary
        thorin::Array<float> host_poses(3 * size);
        for(int i = 0; i < size; ++i) {
            float *pos = &(host_poses.data()[3 * i]);
            const float3 &p = query_poses[i];
            pos[0] = p[0];
            pos[1] = p[1];
            pos[2] = p[2];
        }

        thorin::Array<float> ref_poses = std::move(thorin::Array<float>(thorin::Platform::RANGESEARCH_PLATFORM, thorin::Device(RANGESEARCH_DEVICE), 3 * size));

        thorin::copy(host_poses, ref_poses);

        // Impala API
        return batch_query_hashgrid(hg, ref_poses.data(), size);
    }

private:
    const int CELL_ENDS_SIZE = 1000000;
    PhotonHashGrid* hg = nullptr;
    thorin::Array<float> photon_poses;

};

}

#endif
