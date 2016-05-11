#include <thread>
#include <atomic>

#define ENABLE_QUEUE_STATS
#define ENABLE_MERGING

namespace imba {

/// Launches multiple threads, each running an entire traversal-shading pipeline.
/// Thus, there can be multiple calls to traversal at the same time.
template<typename StateType, int max_shadow_rays_per_hit>
class TileScheduler : public RaySchedulerBase<TileScheduler<StateType, max_shadow_rays_per_hit>, StateType> {
    using BaseType = RaySchedulerBase<TileScheduler<StateType, max_shadow_rays_per_hit>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;

    static const int MIN_QUEUE_SIZE = 0;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

public:
    TileScheduler(RayGen<StateType>& ray_gen, Scene& scene, int num_threads, int tile_size)
        : BaseType(ray_gen, scene)
        , num_threads_(num_threads), tile_size_(tile_size)
        , thread_local_prim_queues_(num_threads * 2)
        , thread_local_shadow_queues_(num_threads)
    {
        // Compute the number of tiles required to cover the entire image.
        tiles_per_row_ = ray_gen_.width() / tile_size_ + (ray_gen_.width() % tile_size_ == 0 ? 0 : 1);
        tiles_per_col_ = ray_gen_.height() / tile_size_ + (ray_gen_.height() % tile_size_ == 0 ? 0 : 1);
        tile_count_ = tiles_per_row_ * tiles_per_col_;

        const auto max_ray_count = tile_size_ * tile_size_ * ray_gen_.num_samples();
        for (auto& q : thread_local_prim_queues_)
            q = new RayQueue<StateType>(max_ray_count);

        for (auto& q : thread_local_shadow_queues_)
            q = new RayQueue<StateType>(max_ray_count * max_shadow_rays_per_hit);

        // Initialize the GPU buffer
        RayQueue<StateType>::setup_device_buffer(max_ray_count * max_shadow_rays_per_hit);

#ifdef ENABLE_QUEUE_STATS
        primary_ray_total = 0;
        shadow_ray_total = 0;
        primary_ray_min = 12345566778;
        shadow_ray_min = 12345566778;
        traversal_calls = 0;
        shadow_traversal_calls = 0;
#endif
    }

    ~TileScheduler() {
#ifdef ENABLE_QUEUE_STATS
        // std::cout << "Queue stats:" << std::endl
        //           << "   primary rays    : " << primary_ray_total << std::endl
        //           << "   shadow rays     : " << shadow_ray_total << std::endl
        //           << "   primary min     : " << primary_ray_min << std::endl
        //           << "   shadow min      : " << shadow_ray_min << std::endl
        //           << "   traversal calls : " << traversal_calls << std::endl
        //           << "   traversal calls (shadow) : " << shadow_traversal_calls << std::endl
        //           << "   average primary : " << primary_ray_total / traversal_calls << std::endl
        //           << "   average shadow  : " << shadow_ray_total / traversal_calls << std::endl;

        std::cout << primary_ray_total + shadow_ray_total << std::endl;
#endif

        RayQueue<StateType>::release_device_buffer();

        for (auto& q : thread_local_prim_queues_)
            delete q;

        for (auto& q : thread_local_shadow_queues_)
            delete q;
    }

    template<typename ShFunc, typename PrimFunc>
    void derived_run_iteration(AtomicImage& image,
                               ShFunc process_shadow_rays,
                               PrimFunc process_primary_rays,
                               SamplePixelFn sample_fn) {
        next_tile_ = 0;

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
    int tile_size_;

    int tile_count_;
    int tiles_per_row_;
    int tiles_per_col_;
    std::atomic<int> next_tile_;

    // Every thread has two primary queues. Thread i owns queue[i * 2] and queue[i * 2 + 1].
    std::vector<RayQueue<StateType>*> thread_local_prim_queues_;

    // Every thread has one shadow queue.
    std::vector<RayQueue<StateType>*> thread_local_shadow_queues_;

#ifdef ENABLE_QUEUE_STATS
    std::atomic<int64_t> primary_ray_total;
    std::atomic<int64_t> shadow_ray_total;
    std::atomic<int64_t> primary_ray_min;
    std::atomic<int64_t> shadow_ray_min;
    std::atomic<int64_t> traversal_calls;
    std::atomic<int64_t> shadow_traversal_calls;
#endif

    inline bool acquire_tile(int tile_id, TiledRayGen<StateType>& out) {
        // Get the next tile and compute its extents
        int tile_pos_x  = (tile_id % tiles_per_row_) * tile_size_;
        int tile_pos_y  = (tile_id / tiles_per_row_) * tile_size_;
        int tile_width  = std::min(ray_gen_.width() - tile_pos_x, tile_size_);
        int tile_height = std::min(ray_gen_.height() - tile_pos_y, tile_size_);

#ifdef ENABLE_MERGING
        // If the next tile is smaller than half the size, acquire it as well.
        // If this tile is smaller than half the size, skip it (was acquired by one of its neighbours)
        if (tile_width < tile_size_ / 2 ||
            tile_height < tile_size_ / 2)
            return false;

        if (ray_gen_.width() - (tile_pos_x + tile_width) < tile_size_ / 2)
            tile_width += ray_gen_.width() - (tile_pos_x + tile_width);

        if (ray_gen_.height() - (tile_pos_y + tile_height) < tile_size_ / 2)
            tile_height += ray_gen_.height() - (tile_pos_y + tile_height);
#endif

        out = TiledRayGen<StateType>(tile_pos_x, tile_pos_y, tile_width, tile_height,
                                     ray_gen_.num_samples(), ray_gen_.width(), ray_gen_.height());
        out.start_frame();

        return true;
    }

    template<typename ShFunc, typename PrimFunc>
    void render_thread(int thread_idx, AtomicImage& image,
                       ShFunc process_shadow_rays,
                       PrimFunc process_primary_rays,
                       SamplePixelFn sample_fn) {
        auto cur_tile = next_tile_++;
        while (cur_tile < tile_count_) {
            TiledRayGen<StateType> tile_ray_gen(0,0,0,0,0,0,0);
            if (!acquire_tile(cur_tile, tile_ray_gen)) {
                cur_tile = next_tile_++;
                continue; // Tile had to be skipped, go for the next one.
            }

            // Get the ray queues for this thread.
            int in_q = 0;
            int out_q = 1;
            auto prim_q_in  = thread_local_prim_queues_[thread_idx * 2 + in_q];
            auto prim_q_out = thread_local_prim_queues_[thread_idx * 2 + out_q];
            auto shadow_q   = thread_local_shadow_queues_[thread_idx];

            // Traverse and shade until there are no more rays left.
            while(!tile_ray_gen.is_empty() || prim_q_in->size() > MIN_QUEUE_SIZE) {
                tile_ray_gen.fill_queue(*prim_q_in, sample_fn);

#ifdef ENABLE_MERGING
                if (prim_q_in->size() < (tile_size_ * tile_size_ * ray_gen_.num_samples()) / 2) {
                    // Acquire another tile, if available, to keep ray count high.
                    cur_tile = next_tile_++;
                    while (cur_tile < tile_count_ && !acquire_tile(cur_tile, tile_ray_gen))
                        cur_tile = next_tile_++;
                }
#endif

#ifdef ENABLE_QUEUE_STATS
                primary_ray_total += prim_q_in->size();
                traversal_calls++;

                int64_t old_val = primary_ray_min;
                while (prim_q_in->size() < old_val && !primary_ray_min.compare_exchange_weak(old_val, prim_q_in->size()))
                    ;
#endif
                prim_q_in->traverse(scene_);

                process_primary_rays(*prim_q_in, *prim_q_out, *shadow_q, image);

                if (shadow_q->size() > MIN_QUEUE_SIZE) {
                    shadow_q->traverse_occluded(scene_);

#ifdef ENABLE_QUEUE_STATS
                    shadow_ray_total += shadow_q->size();
                    shadow_traversal_calls++;

                    int64_t old_val = shadow_ray_min;
                    while (shadow_q->size() < old_val && !shadow_ray_min.compare_exchange_weak(old_val, shadow_q->size()))
                        ;
#endif

                    process_shadow_rays(*shadow_q, image);
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
};

} // namespace imba