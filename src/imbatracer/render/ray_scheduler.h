#ifndef RAY_SCHEDULER
#define RAY_SCHEDULER

#include "ray_gen.h"

#include "tbb/tbb.h"
#include "tbb/task_group.h"

#include <list>
#include <atomic>
#include <cassert>

namespace imba {

template<typename StateType>
class QueueReference {
public:
    QueueReference() : q_(nullptr) {}

    QueueReference(RayQueue<StateType>& q, size_t idx)
        : q_(&q), idx_(idx)
    {}

    size_t index() const { return idx_; }

    RayQueue<StateType>& operator* () const {
        assert(q_ != nullptr);
        return *q_;
    }

    RayQueue<StateType>* operator-> () const {
        assert(q_ != nullptr);
        return q_;
    }

    operator bool() const { return q_ != nullptr; }

private:
    mutable RayQueue<StateType>* q_;
    size_t idx_;
};

enum QueueTag {
    QUEUE_EMPTY,
    QUEUE_IN_USE,
    QUEUE_READY_FOR_TRAVERSAL,
    QUEUE_READY_FOR_SHADOW_TRAVERSAL
};

template<typename StateType, size_t count>
class RayQueuePool {
public:
    RayQueuePool(size_t length) {
        for (auto iter = queue_flags_.begin(); iter != queue_flags_.end(); ++iter)
            iter->store(QUEUE_EMPTY);

        for (auto& q : queues_)
            q = new RayQueue<StateType>(length);

        nonempty_count_ = 0;
    }

    ~RayQueuePool() {
        for (auto& q : queues_)
            delete q;
    }

    /// Finds the next queue that matches the given tag, sets its tag to QUEUE_IN_USE and returns it.
    QueueReference<StateType> claim_queue_with_tag(QueueTag tag) {
        for (size_t i = 0; i < count; ++i) {
            QueueTag expected = tag;
            if (queue_flags_[i].compare_exchange_strong(expected, QUEUE_IN_USE)) {
                // We found a matching queue.
                if (tag == QUEUE_EMPTY)
                    nonempty_count_++;
                return QueueReference<StateType>(*queues_[i], i);
            }
        }

        return QueueReference<StateType>();
    }

    void return_queue(QueueReference<StateType> ref, QueueTag new_tag) {
        // Tag all returned queues that are empty as QUEUE_EMPTY.
        if (ref->size() <= 0)
            new_tag = QUEUE_EMPTY;

        if (new_tag == QUEUE_EMPTY)
            nonempty_count_--;

        queue_flags_[ref.index()] = new_tag;
    }

    /// Checks if there are still any queues left that are not empty.
    bool has_nonempty() { return nonempty_count_ > 0; }

private:
    std::array<RayQueue<StateType>*, count> queues_;
    std::array<std::atomic<QueueTag>, count> queue_flags_;

    std::atomic<size_t> nonempty_count_;
};

/// Schedules the processing of ray queues by the traversal and shading steps.
template<typename StateType, int queue_count, int shadow_queue_count, int max_shadow_rays_per_hit>
class RayScheduler {
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
    static constexpr int DEFAULT_QUEUE_SIZE = 1 << 18;
public:
    RayScheduler(RayGen<StateType>& ray_gen, Scene& scene, int queue_size = DEFAULT_QUEUE_SIZE)
        : ray_gen_(ray_gen)
        , scene_(scene)
        , queue_pool_(queue_size)
        , shadow_queue_pool_(queue_size * max_shadow_rays_per_hit)
    {}

    ~RayScheduler() {}

    template<typename Obj>
    void run_iteration(Image& out, Obj* integrator,
                       void (Obj::*process_shadow_rays)(RayQueue<StateType>& ray_in, Image& out),
                       void (Obj::*process_primary_rays)(RayQueue<StateType>& ray_in, RayQueue<StateType>& ray_out, RayQueue<StateType>& ray_out_shadow, Image& out),
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

            shading_tasks_.run([this, integrator, process_primary_rays, q_ref, &out] () {
                auto q_out = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
                auto q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY); // TODO investigate how to be sure an empty shadow queue exists

                (integrator->*process_primary_rays)(*q_ref, *q_out, *q_out_shadow, out);

                q_ref->clear();
                queue_pool_.return_queue(q_ref, QUEUE_EMPTY);
                queue_pool_.return_queue(q_out, QUEUE_READY_FOR_TRAVERSAL);
                shadow_queue_pool_.return_queue(q_out_shadow, QUEUE_READY_FOR_SHADOW_TRAVERSAL);
            });
        }
    }

private:
    RayQueuePool<StateType, queue_count> queue_pool_;
    RayQueuePool<StateType, shadow_queue_count> shadow_queue_pool_;

    RayGen<StateType>& ray_gen_;
    Scene& scene_;

    tbb::task_group shading_tasks_;
};

}

#endif
