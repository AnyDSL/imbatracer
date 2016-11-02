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

    void build(const Iter& photons_begin, const Iter& photons_end, float radius) {
        std::lock_guard<std::mutex> lock(traversal_mutex);

    sum_cp_q = 0;
    auto t = thorin_get_micro_time();

        // memory allocation and copy onto proper device if necessary
        int photon_count = std::distance(photons_begin, photons_end);	
        thorin::Array<float> host_poses(3 * photon_count);
        
        tbb::parallel_for(tbb::blocked_range<int>(0, photon_count), [&] (const tbb::blocked_range<int>& range) {
            for(auto i = range.begin(); i != range.end(); ++i) {
                Iter it = std::next(photons_begin, i);
                float *pos = &(host_poses.data()[3 * i]);
                const float3 &p = it->position();
                pos[0] = p[0];
                pos[1] = p[1];
                pos[2] = p[2];
            }
        });

        photon_poses = std::move(thorin::Array<float>(thorin::Platform::RANGESEARCH_PLATFORM, thorin::Device(RANGESEARCH_DEVICE), 3 * photon_count));

        thorin::copy(host_poses, photon_poses);

    t = thorin_get_micro_time() - t;
    sum_cp_b += t;
    std::cout << "Build Copy Time: " << t << "(" << sum_cp_b / (++cnt_b) << ")"  << std::endl;
        
        // impala API
	    hg = build_hashgrid(hg, photon_poses.data(), photon_count, CELL_ENDS_SIZE, radius);
    
    }


    BatchQueryResult* process(float3* query_poses, const int size) {
        std::lock_guard<std::mutex> lock(traversal_mutex);
       
    int t = thorin_get_micro_time();

        // memory allocation and copy onto proper device if necessary
        thorin::Array<float> host_poses(3 * size);
        tbb::parallel_for(tbb::blocked_range<int>(0, size), [&] (const tbb::blocked_range<int>& range) {
            for(int i = range.begin(); i != range.end(); ++i) {
                float *pos = &(host_poses.data()[3 * i]);
                const float3 &p = query_poses[i];
                pos[0] = p[0];
                pos[1] = p[1];
                pos[2] = p[2];
            }
        });

        thorin::Array<float> ref_poses = std::move(thorin::Array<float>(thorin::Platform::RANGESEARCH_PLATFORM, thorin::Device(RANGESEARCH_DEVICE), 3 * size));

        thorin::copy(host_poses, ref_poses);

    sum_cp_q += thorin_get_micro_time() - t;
        
        // Impala API
        return batch_query_hashgrid2(hg, ref_poses.data(), size);
    }

private:
    const int CELL_ENDS_SIZE = 1 << 20;
    PhotonHashGrid* hg = nullptr;
    thorin::Array<float> photon_poses;

    int cnt_b = 0; 
    int sum_cp_b = 0, sum_cp_q = 0;

};

}

#endif
