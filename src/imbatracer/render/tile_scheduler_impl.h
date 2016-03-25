#include <thread>
#include <atomic>

namespace imba {

/// Launches multiple threads, each running an entire traversal-shading pipeline.
/// Thus, there can be multiple calls to traversal at the same time.
template<typename StateType, int num_threads, int tile_size>
class TileScheduler : public RaySchedulerBase<TileScheduler<StateType, num_threads, tile_size>, StateType> {
    using BaseType = RaySchedulerBase<TileScheduler<StateType, num_threads, tile_size>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

public:
    TileScheduler(RayGen<StateType>& ray_gen, Scene& scene)
        : RaySchedulerBase<TileScheduler<StateType, num_threads, tile_size>, StateType>(ray_gen, scene)
    {
        // Compute the number of tiles required to cover the entire image.
        tiles_per_row_ = ray_gen_.width() / tile_size + (ray_gen_.width() % tile_size == 0 ? 0 : 1);
        tiles_per_col_ = ray_gen_.height() / tile_size + (ray_gen_.height() % tile_size == 0 ? 0 : 1);
        tile_count_ = tiles_per_row * tiles_per_col;
    }

    template<typename Obj>
    void derived_run_iteration(AtomicImage& out, Obj* integrator,
                               void (Obj::*process_shadow_rays)(RayQueue<StateType>&, AtomicImage&),
                               void (Obj::*process_primary_rays)(RayQueue<StateType>&, RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&),
                               SamplePixelFn sample_fn) {
        next_tile_ = 0;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([this](){render_thread();});
        }

        for (auto& t : threads)
            t.join();
    }

private:
    int tile_count_;
    int tiles_per_row_;
    int tiles_per_col_;
    std::atomic<int> next_tile_;

    void render_thread() {
        RayQueue<StateType> primary_queues[2];
        RayQueue<StateType> shadow_queue;
        int in_q = 0;

        while (++next_tile_ < tile_count_) {
            // Get the next tile and compute its extents
            int tile_pos_x  = (tile_count / tiles_per_row_) * tile_size;
            int tile_pos_y  = (tile_count % tiles_per_row_) * tile_size;
            int tile_width  = std::min(ray_gen_.width() - tile_pos_x, tile_size);
            int tile_height = std::min(ray_gen_.height() - tile_pos_y, tile_size);

            TiledRayGen<StateType> tile_ray_gen(tile_pos_x, tile_pos_y, tile_width, tile_height, ray_gen_.num_samples());

            // Render the tile

        }
    }
};

} // namespace imba