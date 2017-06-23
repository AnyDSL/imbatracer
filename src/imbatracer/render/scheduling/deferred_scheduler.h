#ifndef IMBA_DEFERRED_SCHEDULER_H
#define IMBA_DEFERRED_SCHEDULER_H

#include "imbatracer/core/image.h"
#include "imbatracer/render/scene.h"
#include "imbatracer/render/ray_gen/tile_gen.h"
#include "imbatracer/render/scheduling/queue_pool.h"

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

namespace imba {

/// An adapted version of the \see{TileScheduler} to work best with the DeferredVCM Integrator.
template <typename StateType, bool occluded = false>
class DeferredScheduler {
    const bool gpu_traversal;

public:
    using SampleFn      = typename RayGen<StateType>::SampleFn;
    using ShadeFn       = typename std::function<void (Ray&, Hit&, StateType&)>;
    using ShadeEmptyFn  = typename std::function<void (Ray&, StateType&)>;

    DeferredScheduler(const Scene* scene,
                      int num_qs,
                      int q_size,
                      bool gpu_traversal)
        : scene_(scene)
        , gpu_traversal(gpu_traversal)
        , queue_pool_(q_size, num_qs, gpu_traversal)
    {
    }

    void run_iteration(TileGen<StateType>* tile_gen,
                       ShadeEmptyFn shade_empties,
                       ShadeFn shade_hits,
                       SampleFn sample_fn) {
        tile_gen->start_frame();
        avail_qs_ = queue_pool_.size();
        tbb::parallel_for(tbb::blocked_range<int>(0, tile_gen->num_tiles()),
        [&] (const tbb::blocked_range<int>& range)
        {
            for (auto i = range.begin(); i != range.end(); ++i) {
                render_tile(tile_gen, i, shade_empties, shade_hits, sample_fn);
            }
        });
    }

private:
    const Scene* scene_;

    RayQueuePool<StateType> queue_pool_;
    std::condition_variable cv_;
    std::mutex mutex_;
    int avail_qs_;

    void render_tile(TileGen<StateType>* tile_gen, int tile, ShadeEmptyFn shade_empties, ShadeFn shade_hits, SampleFn sample_fn) {
        auto q = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
        while (!q) {
            std::unique_lock<std::mutex> lock(mutex_);
            while (avail_qs_ <= 0) cv_.wait(lock);
            avail_qs_--;
            q = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
        }

        uint8_t buf[max_ray_gen_size<StateType>()];
        auto cur_tile = tile_gen->get_tile(tile, buf);

        assert(cur_tile != nullptr);
        assert(q->size() == 0);

        // Traverse and shade until there are no more rays left.
        cur_tile->start_frame();

        while (!cur_tile->is_empty() || q->size() > 0) {
            cur_tile->fill_queue(*q, sample_fn);
            if (q->size() == 0) break;

            if (occluded) {
                if (gpu_traversal) q->traverse_occluded_gpu(scene_->traversal_data_gpu());
                else               q->traverse_occluded_cpu(scene_->traversal_data_cpu());
            } else {
                if (gpu_traversal) q->traverse_gpu(scene_->traversal_data_gpu());
                else               q->traverse_cpu(scene_->traversal_data_cpu());
            }

            const int hit_count = q->compact_hits();

            if (shade_empties) {
                tbb::parallel_for(tbb::blocked_range<int>(hit_count, q->size()),
                [&] (const tbb::blocked_range<int>& range)
                {
                    for (auto i = range.begin(); i != range.end(); ++i) {
                        shade_empties(q->ray(i), q->state(i));
                    }
                });
            }

            if (shade_hits) {
                q->sort_by_material([this](const Hit& hit){ return scene_->mat_id(hit); },
                                    scene_->material_count(), hit_count);
                q->shrink(hit_count);

                tbb::parallel_for(tbb::blocked_range<int>(0, q->size()),
                [&] (const tbb::blocked_range<int>& range)
                {
                    for (auto i = range.begin(); i != range.end(); ++i) {
                        shade_hits(q->ray(i), q->hit(i), q->state(i));
                    }
                });

                q->compact_rays();
            } else {
                // If hits are not shaded: Delete all rays in the queue.
                q->clear();
            }
        }

        // Force deletion, memory will be reused!
        cur_tile.reset(nullptr);

        // Return the queue
        queue_pool_.return_queue(q, QUEUE_EMPTY);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            avail_qs_++;
        }
        cv_.notify_all();
    }
};

} // namespace imba

#endif //IMBA_DEFERRED_SCHEDULER_H