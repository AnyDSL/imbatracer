#ifndef IMBA_TILE_SCHEDULER_H
#define IMBA_TILE_SCHEDULER_H

#include "ray_scheduler.h"

#include <thread>
#include <atomic>

namespace imba {

/// Launches multiple threads, each running an entire traversal-shading pipeline.
/// Thus, there can be multiple calls to traversal at the same time.
template <typename StateType>
class TileScheduler : public RayScheduler<StateType> {
    using BaseType = RayScheduler<StateType>;
    using SamplePixelFn = typename RayScheduler<StateType>::SamplePixelFn;
    using ProcessPrimaryFn = typename RayScheduler<StateType>::ProcessPrimaryFn;
    using ProcessShadowFn = typename RayScheduler<StateType>::ProcessShadowFn;

    static constexpr int MIN_QUEUE_SIZE = 0;

protected:
    using BaseType::scene_;

public:
    TileScheduler(TileGen<StateType>& tile_gen,
                  Scene& scene,
                  int max_shadow_rays_per_hit,
                  int num_threads, int q_size)
        : BaseType(scene)
        , tile_gen_(tile_gen)
        , num_threads_(num_threads), q_size_(q_size)
        , thread_local_prim_queues_(num_threads * 2)
        , thread_local_shadow_queues_(num_threads)
    {
        for (auto& q : thread_local_prim_queues_)
            q = new RayQueue<StateType>(q_size);

        for (auto& q : thread_local_shadow_queues_)
            q = new RayQueue<StateType>(q_size * max_shadow_rays_per_hit);

        // Initialize the GPU buffer
        RayQueue<StateType>::setup_device_buffer(q_size * max_shadow_rays_per_hit);
    }

    ~TileScheduler() {
        RayQueue<StateType>::release_device_buffer();

        for (auto& q : thread_local_prim_queues_)
            delete q;

        for (auto& q : thread_local_shadow_queues_)
            delete q;
    }

    void run_iteration(AtomicImage& image,
                       ProcessShadowFn process_shadow_rays,
                       ProcessPrimaryFn process_primary_rays,
                       SamplePixelFn sample_fn) override final {
        tile_gen_.start_frame();

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads_; ++i) {
            threads.emplace_back([this, i, &image, process_shadow_rays, process_primary_rays, sample_fn]()
                {
                    render_thread(i, image, process_shadow_rays, process_primary_rays, sample_fn);
                });
        }

        for (auto& t : threads)
            t.join();
    }

private:
    int num_threads_;
    int q_size_;

    TileGen<StateType>& tile_gen_;

    // Every thread has two primary queues. Thread i owns queue[i * 2] and queue[i * 2 + 1].
    std::vector<RayQueue<StateType>*> thread_local_prim_queues_;

    // Every thread has one shadow queue.
    std::vector<RayQueue<StateType>*> thread_local_shadow_queues_;

    void render_thread(int thread_idx, AtomicImage& image,
                       ProcessShadowFn process_shadow_rays,
                       ProcessPrimaryFn process_primary_rays,
                       SamplePixelFn sample_fn) {
        std::unique_ptr<RayGen<StateType> > cur_tile;
        while ((cur_tile = tile_gen_.next_tile()) != nullptr ) {
            // Get the ray queues for this thread.
            int in_q = 0;
            int out_q = 1;
            auto prim_q_in  = thread_local_prim_queues_[thread_idx * 2 + in_q];
            auto prim_q_out = thread_local_prim_queues_[thread_idx * 2 + out_q];
            auto shadow_q   = thread_local_shadow_queues_[thread_idx];

            // Traverse and shade until there are no more rays left.
            cur_tile->start_frame();
            while(!cur_tile->is_empty() || prim_q_in->size() > MIN_QUEUE_SIZE) {
                cur_tile->fill_queue(*prim_q_in, sample_fn);

                // TODO Add regeneration again (minor performance increase)

                prim_q_in->traverse(scene_.traversal_data());

                process_primary_rays(*prim_q_in, *prim_q_out, *shadow_q, image);

                if (shadow_q->size() > MIN_QUEUE_SIZE) {
                    shadow_q->traverse_occluded(scene_.traversal_data());

                    process_shadow_rays(*shadow_q, image);
                }

                shadow_q->clear();
                prim_q_in->clear();

                std::swap(in_q, out_q);
                prim_q_in  = thread_local_prim_queues_[thread_idx * 2 + in_q];
                prim_q_out = thread_local_prim_queues_[thread_idx * 2 + out_q];
            }
        }
    }
};

} // namespace imba

#endif // IMBA_TILE_SCHEDULER_H