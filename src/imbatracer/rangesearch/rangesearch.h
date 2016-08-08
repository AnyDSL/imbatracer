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

    void build(const Iter& photons_begin, const Iter& photons_end, float radius) {
        std::lock_guard<std::mutex> lock(traversal_mutex);
        if (hg) {
            destroy_hashgrid(hg);
            delete hg;
            hg = nullptr;
        }

	    int photon_count = std::distance(photons_begin, photons_end);	

	    struct RawDataInfo rdi;
	    rdi.begin = &(photons_begin->position()[0]);
        rdi.stride = sizeof(*photons_begin) / sizeof(float);
        
        // Impala API
	    hg = build_hashgrid(&rdi, photon_count, CELL_ENDS_SIZE, radius);
    }


    BatchQueryResult* process(float3* query_poses, const int size) {
        std::lock_guard<std::mutex> lock(traversal_mutex);
        // Impala API
        return batch_query_hashgrid(hg, &(query_poses[0].x), size);
    }

private:
    const int CELL_ENDS_SIZE = 1000000;
    PhotonHashGrid* hg = nullptr;

};

}

#endif
