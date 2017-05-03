#ifndef IMBA_TILE_SCHEDULER_H
#define IMBA_TILE_SCHEDULER_H

#include "ray_scheduler.h"

#include <thread>
#include <atomic>

#define RAY_STATISTICS

namespace imba {

/// Launches multiple threads, each running an entire traversal-shading pipeline.
/// Thus, there can be multiple calls to traversal at the same time.
template <typename StateType, typename ShadowStateType>
class TileScheduler : public RayScheduler<StateType, ShadowStateType> {
    using BaseType = RayScheduler<StateType, ShadowStateType>;
    using SamplePixelFn = typename BaseType::SamplePixelFn;
    using ProcessPrimaryFn = typename BaseType::ProcessPrimaryFn;
    using ProcessShadowFn = typename BaseType::ProcessShadowFn;

    static constexpr int MIN_QUEUE_SIZE = 0;

protected:
    using BaseType::scene_;
    using BaseType::gpu_traversal;

public:
    TileScheduler(TileGen<StateType>& tile_gen,
                  Scene& scene,
                  int max_shadow_rays_per_hit,
                  int num_threads, int q_size,
                  bool gpu_traversal = true)
        : BaseType(scene, gpu_traversal)
        , tile_gen_(tile_gen)
        , num_threads_(num_threads), q_size_(q_size)
        , thread_local_prim_queues_(num_threads)
        , thread_local_shadow_queues_(num_threads)
        , thread_local_ray_gen_(num_threads)
    {
        for (auto& q : thread_local_prim_queues_)
            q = new RayQueue<StateType>(q_size, gpu_traversal);

        for (auto& q : thread_local_shadow_queues_)
            q = new RayQueue<ShadowStateType>(q_size * max_shadow_rays_per_hit, gpu_traversal);

        for (auto& ptr : thread_local_ray_gen_)
            ptr = new uint8_t[tile_gen_.sizeof_ray_gen()];

        total_prim_rays_   = 0;
        total_shadow_rays_ = 0;
    }

    ~TileScheduler() {
        for (auto q : thread_local_prim_queues_) delete q;
        for (auto q : thread_local_shadow_queues_) delete q;

        for (auto ptr : thread_local_ray_gen_) delete [] ptr;

#ifdef RAY_STATISTICS
        std::cout << "Number primary rays: " << total_prim_rays_ << " Number shadow rays: " << total_shadow_rays_ << std::endl;
#endif
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
    std::vector<RayQueue<ShadowStateType>*> thread_local_shadow_queues_;

    // Every thread has a ray generator.
    // To prevent reallocation every time a new tile is needed, we use a memory pool.
    std::vector<uint8_t*> thread_local_ray_gen_;

#ifdef RAY_STATISTICS
    std::atomic<uint64_t> total_prim_rays_;
    std::atomic<uint64_t> total_shadow_rays_;
#endif

    void render_thread(int thread_idx, AtomicImage& image,
                       ProcessShadowFn process_shadow_rays,
                       ProcessPrimaryFn process_primary_rays,
                       SamplePixelFn sample_fn) {
        auto cur_tile = tile_gen_.next_tile(thread_local_ray_gen_[thread_idx]);
        while (cur_tile != nullptr) {
            // Get the ray queues for this thread.
            auto prim_q   = thread_local_prim_queues_  [thread_idx];
            auto shadow_q = thread_local_shadow_queues_[thread_idx];

            // Traverse and shade until there are no more rays left.
            cur_tile->start_frame();
            while(!cur_tile->is_empty() || prim_q->size() > MIN_QUEUE_SIZE) {
                cur_tile->fill_queue(*prim_q, sample_fn);

                // TODO Add regeneration again (minor performance increase)

#ifdef RAY_STATISTICS
                total_prim_rays_ += prim_q->size();
#endif

                if (gpu_traversal) prim_q->traverse_gpu(scene_.traversal_data_gpu());
                else               prim_q->traverse_cpu(scene_.traversal_data_cpu());

                process_primary_rays(*prim_q, *shadow_q, image);

                if (shadow_q->size() > MIN_QUEUE_SIZE) {
#ifdef RAY_STATISTICS
                total_shadow_rays_ += shadow_q->size();
#endif
                    if (gpu_traversal)
                        shadow_q->traverse_occluded_gpu(scene_.traversal_data_gpu());
                    else
                        shadow_q->traverse_occluded_cpu(scene_.traversal_data_cpu());
                    process_shadow_rays(*shadow_q, image);
                }

                shadow_q->clear();
            }

            // We are using the same memory for the new ray generation, so we
            // have to delete the old one first!
            cur_tile.reset(nullptr);
            cur_tile = tile_gen_.next_tile(thread_local_ray_gen_[thread_idx]);
        }
    }
};

} // namespace imba

#endif // IMBA_TILE_SCHEDULER_H
