
namespace imba {

/// Uses a fixed number of queues, a single traversal and multiple shading threads.
template<typename StateType, int queue_count, int shadow_queue_count, int max_shadow_rays_per_hit>
class QueueScheduler : public RaySchedulerBase<QueueScheduler<StateType, queue_count, shadow_queue_count, max_shadow_rays_per_hit>, StateType> {
    using BaseType = RaySchedulerBase<QueueScheduler<StateType, queue_count, shadow_queue_count, max_shadow_rays_per_hit>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
    static constexpr int DEFAULT_QUEUE_SIZE = 1 << 16;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

public:
    QueueScheduler(RayGen<StateType>& ray_gen, Scene& scene, int queue_size = DEFAULT_QUEUE_SIZE)
        : RaySchedulerBase<QueueScheduler<StateType, queue_count, shadow_queue_count, max_shadow_rays_per_hit>, StateType>(ray_gen, scene)
        , queue_pool_(queue_size)
        , shadow_queue_pool_(queue_size * max_shadow_rays_per_hit)
    {}

    ~QueueScheduler() {}

    template<typename Obj>
    void derived_run_iteration(AtomicImage& out, Obj* integrator,
                               void (Obj::*process_shadow_rays)(RayQueue<StateType>&, AtomicImage&),
                               void (Obj::*process_primary_rays)(RayQueue<StateType>&, RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&),
                               SamplePixelFn sample_fn) {
        ray_gen_.start_frame();

        while (queue_pool_.has_nonempty() ||
               shadow_queue_pool_.has_nonempty() ||
               !ray_gen_.is_empty()) {
            // Traverse the next set of rays.
            // Prioritize shadow rays as they are faster to traverse.
            auto q_ref = shadow_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_SHADOW_TRAVERSAL);
            if (q_ref) {
                q_ref->traverse_occluded(scene_);

                (integrator->*process_shadow_rays)(*q_ref, out);

                q_ref->clear();
                shadow_queue_pool_.return_queue(q_ref, QUEUE_EMPTY);

                continue;
            }

            // There are no shadow ray queues left. Try to find a queue of primary rays.
            q_ref = queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_TRAVERSAL);
            if (!q_ref) {
                // There is no queue of primary rays ready for traversal
                if (!ray_gen_.is_empty()) {
                    q_ref = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
                    if (q_ref) {
                        // We found an empty queue. Fill it with a set of rays from the ray generation (camera or light).
                        ray_gen_.fill_queue(*q_ref, sample_fn);
                    }
                }
            }

            if (!q_ref) {
                // There is no work for traversal at the moment.
                shading_tasks_.wait();
                continue;
            }

            // Traverse the queue of primary rays.
            q_ref->traverse(scene_);

            auto q_out = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            if (!q_out) {
                // All queues are currently occupied as input or output of shading tasks.
                shading_tasks_.wait();
                continue;
            }
            auto q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);

            shading_tasks_.run([this, integrator, process_primary_rays, q_ref, q_out, q_out_shadow, &out] () {
                (integrator->*process_primary_rays)(*q_ref, *q_out, *q_out_shadow, out);

                q_ref->clear();
                queue_pool_.return_queue(q_ref, QUEUE_EMPTY);
                queue_pool_.return_queue(q_out, QUEUE_READY_FOR_TRAVERSAL);
                shadow_queue_pool_.return_queue(q_out_shadow, QUEUE_READY_FOR_SHADOW_TRAVERSAL);
            });
        }

        shading_tasks_.wait();
    }

private:
    RayQueuePool<StateType, queue_count> queue_pool_;
    RayQueuePool<StateType, shadow_queue_count> shadow_queue_pool_;

    tbb::task_group shading_tasks_;
};

} // namespace imba
