#include <thread>
#include <mutex>
#include <atomic>

namespace imba {

/// Launches multiple threads, each running an entire traversal-shading pipeline.
/// Thus, there can be multiple calls to traversal at the same time.
template<typename StateType, int num_threads, int tile_size, int max_shadow_rays_per_hit, bool single_traversal = true>
class TileScheduler : public RaySchedulerBase<TileScheduler<StateType, num_threads, tile_size, max_shadow_rays_per_hit, single_traversal>, StateType> {
    using BaseType = RaySchedulerBase<TileScheduler<StateType, num_threads, tile_size, max_shadow_rays_per_hit, single_traversal>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

public:
    TileScheduler(RayGen<StateType>& ray_gen, Scene& scene)
        : RaySchedulerBase<TileScheduler<StateType, num_threads, tile_size, max_shadow_rays_per_hit, single_traversal>, StateType>(ray_gen, scene)
    {
        // Compute the number of tiles required to cover the entire image.
        tiles_per_row_ = ray_gen_.width() / tile_size + (ray_gen_.width() % tile_size == 0 ? 0 : 1);
        tiles_per_col_ = ray_gen_.height() / tile_size + (ray_gen_.height() % tile_size == 0 ? 0 : 1);
        tile_count_ = tiles_per_row_ * tiles_per_col_;

        for (auto& q : thread_local_prim_queues_)
            q = new RayQueue<StateType>(tile_size * tile_size * ray_gen_.num_samples());

        for (auto& q : thread_local_shadow_queues_)
            q = new RayQueue<StateType>(tile_size * tile_size * ray_gen_.num_samples() * max_shadow_rays_per_hit);
    }

    ~TileScheduler() {
        for (auto& q : thread_local_prim_queues_)
            delete q;

        for (auto& q : thread_local_shadow_queues_)
            delete q;
    }

    template<typename Obj>
    void derived_run_iteration(AtomicImage& image, Obj* integrator,
                               void (Obj::*process_shadow_rays)(RayQueue<StateType>&, AtomicImage&),
                               void (Obj::*process_primary_rays)(RayQueue<StateType>&, RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&),
                               SamplePixelFn sample_fn) {
        next_tile_ = 0;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([this, i, &image, integrator, process_shadow_rays, process_primary_rays, sample_fn]()
                {
                    render_thread(i, image, integrator, process_shadow_rays, process_primary_rays, sample_fn);
                });
        }

        for (auto& t : threads)
            t.join();
    }

private:
    int tile_count_;
    int tiles_per_row_;
    int tiles_per_col_;
    std::atomic<int> next_tile_;

    // Every thread has two primary queues. Thread i owns queue[i * 2] and queue[i * 2 + 1].
    std::array<RayQueue<StateType>*, num_threads * 2> thread_local_prim_queues_;

    // Every thread has one shadow queue.
    std::array<RayQueue<StateType>*, num_threads> thread_local_shadow_queues_;

    template<typename Obj>
    void render_thread(int thread_idx, AtomicImage& image, Obj* integrator,
                       void (Obj::*process_shadow_rays)(RayQueue<StateType>&, AtomicImage&),
                       void (Obj::*process_primary_rays)(RayQueue<StateType>&, RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&),
                       SamplePixelFn sample_fn) {
        auto cur_tile = next_tile_++;
        while (cur_tile < tile_count_) {
            // Get the next tile and compute its extents
            int tile_pos_x  = (cur_tile % tiles_per_row_) * tile_size;
            int tile_pos_y  = (cur_tile / tiles_per_row_) * tile_size;
            int tile_width  = std::min(ray_gen_.width() - tile_pos_x, tile_size);
            int tile_height = std::min(ray_gen_.height() - tile_pos_y, tile_size);

            TiledRayGen<StateType> tile_ray_gen(tile_pos_x, tile_pos_y, tile_width, tile_height,
                ray_gen_.num_samples(), ray_gen_.width(), ray_gen_.height());

            tile_ray_gen.start_frame();

            // Get the ray queues for this thread.
            int in_q = 0;
            int out_q = 1;
            auto prim_q_in  = thread_local_prim_queues_[thread_idx * 2 + in_q];
            auto prim_q_out = thread_local_prim_queues_[thread_idx * 2 + out_q];
            auto shadow_q   = thread_local_shadow_queues_[thread_idx];

            // Traverse and shade until there are no more rays left.
            while(!tile_ray_gen.is_empty() || prim_q_in->size() > 0) {
                tile_ray_gen.fill_queue(*prim_q_in, sample_fn);

                if (single_traversal) {
                    std::lock_guard<std::mutex> lock(traversal_mutex_);
                    prim_q_in->traverse(scene_);
                } else
                    prim_q_in->traverse(scene_);

                (integrator->*process_primary_rays)(*prim_q_in, *prim_q_out, *shadow_q, image);

                if (shadow_q->size() > 0) {
                    if (single_traversal) {
                        std::lock_guard<std::mutex> lock(traversal_mutex_);
                        shadow_q->traverse_occluded(scene_);
                    } else
                        shadow_q->traverse_occluded(scene_);

                    (integrator->*process_shadow_rays)(*shadow_q, image);
                }

                shadow_q->clear();
                prim_q_in->clear();

                std::swap(in_q, out_q);
                prim_q_in  = thread_local_prim_queues_[thread_idx * 2 + in_q];
                prim_q_out = thread_local_prim_queues_[thread_idx * 2 + out_q];
            }
            cur_tile = next_tile_++;
        }
    }

    std::mutex traversal_mutex_;
};

} // namespace imba