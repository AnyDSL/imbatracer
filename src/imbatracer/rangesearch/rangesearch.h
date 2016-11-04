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

    int get_query_time_count() { return (int)hg->time_count; }
    int get_query_copy_time() { return sum_cp_q; }

    void build(thorin::Array<float> &host_poses, const int size, float radius) {
        std::lock_guard<std::mutex> lock(traversal_mutex);

    sum_cp_q = 0;
    auto t = thorin_get_micro_time();

        // memory allocation and copy onto proper device if necessary
        if (thorin::Platform::RANGESEARCH_PLATFORM != thorin::Platform::HOST) {
            thorin::Array<float> dummy_poses(3 * size);
            dev_photon_poses = std::move(thorin::Array<float>(thorin::Platform::RANGESEARCH_PLATFORM, thorin::Device(RANGESEARCH_DEVICE), 3 * size));

            thorin::copy(host_poses, 0, dev_photon_poses, 0, 3 * size);

    t = thorin_get_micro_time() - t;
    sum_cp_b += t;
    std::cout << "Build Copy Time: " << t << "(" << sum_cp_b / (++cnt_b) << ")"  << std::endl;
	        
            hg = build_hashgrid(hg, dev_photon_poses.data(), size, CELL_ENDS_SIZE, radius);
        }
        else {
    t = thorin_get_micro_time() - t;
    sum_cp_b += t;
    std::cout << "Build Copy Time: " << t << "(" << sum_cp_b / (++cnt_b) << ")"  << std::endl;
            hg = build_hashgrid(hg, host_poses.data(), size, CELL_ENDS_SIZE, radius);
        } 
    }
 
    BatchQueryResult* process(thorin::Array<float> &host_poses, const int size) {
        std::lock_guard<std::mutex> lock(traversal_mutex);
       
    int t = thorin_get_micro_time();
        
        // memory allocation and copy onto proper device if necessary
        if (thorin::Platform::RANGESEARCH_PLATFORM != thorin::Platform::HOST) {
            thorin::Array<float> dev_poses = std::move(thorin::Array<float>(thorin::Platform::RANGESEARCH_PLATFORM, thorin::Device(RANGESEARCH_DEVICE), 3 * size));

            thorin::copy(host_poses, 0, dev_poses, 0, 3 * size);
    sum_cp_q += thorin_get_micro_time() - t;
            return batch_query_hashgrid2(hg, dev_poses.data(), size);
        }
        else {
    sum_cp_q += thorin_get_micro_time() - t;
            return batch_query_hashgrid2(hg, host_poses.data(), size);
        }
    }


private:
    const int CELL_ENDS_SIZE = 1 << 20;
    PhotonHashGrid* hg = nullptr;
    thorin::Array<float> dev_photon_poses;

    int cnt_b = 0; 
    int sum_cp_b = 0, sum_cp_q = 0;

};

}

#endif
