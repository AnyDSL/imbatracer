#ifndef IMBA_DEFERRED_SCHEDULER_H
#define IMBA_DEFERRED_SCHEDULER_H

#include "imbatracer/core/image.h"
#include "imbatracer/render/scene.h"
#include "imbatracer/render/ray_gen/tile_gen.h"

#include <thread>
#include <atomic>

namespace imba {

/// An adapted version of the \see{TileScheduler} to work best with the DeferredVCM Integrator.
template <typename StateType>
class DeferredScheduler {
    const bool gpu_traversal;

public:
    using SampleFn      = typename RayGen<StateType>::SampleFn;
    using ShadeFn       = typename std::function<void (Ray&, Hit&, StateType&, AtomicImage&)>;
    using ShadeEmptyFn  = typename std::function<void (Ray&, StateType&, AtomicImage&)>;

    DeferredScheduler(const Scene* scene,
                      int num_threads,
                      int q_size,
                      bool gpu_traversal,
                      int max_ray_gen_size)
        : scene_(scene)
        , gpu_traversal(gpu_traversal)
        , num_threads_(num_threads), q_size_(q_size)
        , thread_local_q_(num_threads)
        , thread_local_ray_gen_(num_threads)
    {
        for (auto& q : thread_local_q_)
            q = new RayQueue<StateType>(q_size, gpu_traversal);

        for (auto& ptr : thread_local_ray_gen_)
            ptr = new uint8_t[max_ray_gen_size];
    }

    ~DeferredScheduler() {
        for (auto q : thread_local_q_) delete q;
        for (auto ptr : thread_local_ray_gen_) delete [] ptr;
    }

    void run_iteration(TileGen<StateType>* tile_gen,
                       AtomicImage& image,
                       ShadeEmptyFn shade_empties,
                       ShadeFn shade_hits,
                       SampleFn sample_fn) {
        tile_gen->start_frame();

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads_; ++i) {
            threads.emplace_back([this, i, &image, shade_empties, shade_hits, sample_fn, tile_gen]()
                {
                    render_thread(i, image, shade_empties, shade_hits, sample_fn, tile_gen);
                });
        }

        for (auto& t : threads)
            t.join();
    }

private:
    const Scene* scene_;
    int num_threads_;
    int q_size_;

    std::vector<RayQueue<StateType>*> thread_local_q_;

    /// Thread local memory pool for the ray generation.
    std::vector<uint8_t*> thread_local_ray_gen_;

    void render_thread(int thread_idx, AtomicImage& image, ShadeEmptyFn shade_empties, ShadeFn shade_hits, SampleFn sample_fn, TileGen<StateType>* tile_gen) {
        auto cur_tile = tile_gen->next_tile(thread_local_ray_gen_[thread_idx]);
        while (cur_tile != nullptr) {
            // Get the ray queues for this thread.
            auto q = thread_local_q_[thread_idx];

            // Traverse and shade until there are no more rays left.
            cur_tile->start_frame();
            while(!cur_tile->is_empty() || q->size() > 0) {
                cur_tile->fill_queue(*q, sample_fn);

                if (gpu_traversal) q->traverse_gpu(scene_->traversal_data_gpu());
                else               q->traverse_cpu(scene_->traversal_data_cpu());

                const int hit_count = q->compact_hits();
                q->sort_by_material([this](const Hit& hit){ return scene_->mat_id(hit); },
                                    scene_->material_count(), hit_count);

                if (shade_empties) {
                    tbb::parallel_for(tbb::blocked_range<int>(hit_count, q->size()),
                    [&] (const tbb::blocked_range<int>& range)
                    {
                        for (auto i = range.begin(); i != range.end(); ++i) {
                            shade_empties(q->ray(i), q->state(i), image);
                        }
                    });
                }

                if (shade_hits) {
                    q->shrink(hit_count);

                    tbb::parallel_for(tbb::blocked_range<int>(0, q->size()),
                    [&] (const tbb::blocked_range<int>& range)
                    {
                        for (auto i = range.begin(); i != range.end(); ++i) {
                            shade_hits(q->ray(i), q->hit(i), q->state(i), image);
                        }
                    });

                    q->compact_rays();
                } else {
                    // If hits are not shaded: Delete all rays in the queue.
                    q->clear();
                }
            }

            // We are using the same memory for the new ray generation, so we
            // have to delete the old one first!
            cur_tile.reset(nullptr);
            cur_tile = tile_gen->next_tile(thread_local_ray_gen_[thread_idx]);
        }
    }
};

} // namespace imba

#endif //IMBA_DEFERRED_SCHEDULER_H