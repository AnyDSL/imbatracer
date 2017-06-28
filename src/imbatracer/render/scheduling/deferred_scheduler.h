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
public:
    using SampleFn      = typename RayGen<StateType>::SampleFn;
    using ShadeFn       = typename std::function<void (Ray&, Hit&, StateType&)>;
    using ShadeEmptyFn  = typename std::function<void (Ray&, StateType&)>;

    DeferredScheduler(const Scene* scene, int q_size, bool gpu_traversal)
        : scene_(scene)
        , gpu_traversal_(gpu_traversal)
        , q_size_(q_size)
    {}

    void run_iteration(TileGen<StateType>* tile_gen,
                       ShadeEmptyFn shade_empties,
                       ShadeFn shade_hits,
                       SampleFn sample_fn) {
        tile_gen->start_frame();
        ProcessTile process_tile(scene_, q_size_, gpu_traversal_, tile_gen, shade_empties, shade_hits, sample_fn);
        tbb::parallel_for(tbb::blocked_range<int>(0, tile_gen->num_tiles()), process_tile);
    }

private:
    const Scene* scene_;
    const bool gpu_traversal_;
    const int q_size_;

    struct ProcessTile {
        mutable RayQueue<StateType> q;

        const Scene*        scene;
        TileGen<StateType>* tile_gen;
        bool                gpu_traversal;

        ShadeEmptyFn    shade_empties;
        ShadeFn         shade_hits;
        SampleFn        sample_fn;

        ProcessTile(const Scene* scene,
                    int q_size, bool gpu_traversal,
                    TileGen<StateType>* tile_gen,
                    ShadeEmptyFn shade_empties,
                    ShadeFn shade_hits,
                    SampleFn sample_fn)
            : q(q_size, gpu_traversal)
            , scene(scene)
            , tile_gen(tile_gen)
            , gpu_traversal(gpu_traversal)
            , shade_empties(shade_empties)
            , shade_hits(shade_hits)
            , sample_fn(sample_fn)
        {}

        ProcessTile(const ProcessTile& other)
            : q(other.q.capacity(), other.gpu_traversal)
            , scene(other.scene)
            , tile_gen(other.tile_gen)
            , gpu_traversal(other.gpu_traversal)
            , shade_empties(other.shade_empties)
            , shade_hits(other.shade_hits)
            , sample_fn(other.sample_fn)
        {}

        void operator () (const tbb::blocked_range<int> range) const {
            for (auto i = range.begin(), last = range.end(); i != last; ++i) render_tile(i, last);
        }

        void render_tile(int tile, int last_tile) const {
            uint8_t buf[max_ray_gen_size<StateType>()];
            auto cur_tile = tile_gen->get_tile(tile, buf);
            assert(cur_tile != nullptr);

            // Traverse and shade until there are no more rays left.
            cur_tile->start_frame();
            while (!cur_tile->is_empty() || (tile == last_tile - 1 && q.size() > 0)) {
                cur_tile->fill_queue(q, sample_fn);
                if (q.size() == 0) break;

                if (occluded) {
                    if (gpu_traversal) q.traverse_occluded_gpu(scene->traversal_data_gpu());
                    else               q.traverse_occluded_cpu(scene->traversal_data_cpu());
                } else {
                    if (gpu_traversal) q.traverse_gpu(scene->traversal_data_gpu());
                    else               q.traverse_cpu(scene->traversal_data_cpu());
                }

                const int hit_count = q.compact_hits();

                if (shade_empties) {
                    tbb::parallel_for(tbb::blocked_range<int>(hit_count, q.size()),
                    [&] (const tbb::blocked_range<int>& range)
                    {
                        for (auto i = range.begin(); i != range.end(); ++i) {
                            shade_empties(q.ray(i), q.state(i));
                        }
                    });
                }

                if (shade_hits) {
                    q.sort_by_material([this](const Hit& hit){ return scene->mat_id(hit); },
                                        scene->material_count(), hit_count);
                    q.shrink(hit_count);

                    tbb::parallel_for(tbb::blocked_range<int>(0, q.size()),
                    [&] (const tbb::blocked_range<int>& range)
                    {
                        for (auto i = range.begin(); i != range.end(); ++i) {
                            shade_hits(q.ray(i), q.hit(i), q.state(i));
                        }
                    });

                    q.compact_rays();
                } else {
                    // If hits are not shaded: Delete all rays in the queue.
                    q.clear();
                }
            }

            // Force deletion, memory will be reused!
            cur_tile.reset(nullptr);
        }
    };

};

} // namespace imba

#endif //IMBA_DEFERRED_SCHEDULER_H