#ifndef RAY_SCHEDULER
#define RAY_SCHEDULER

#include "ray_gen.h"

#include <tbb/tbb.h>
#include <tbb/task_group.h>

#include <array>
#include <atomic>
#include <cassert>
#include <list>

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

/// Base class for all types of schedulers.
template<typename Derived, typename StateType>
class RaySchedulerBase {
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
public:
    RaySchedulerBase(RayGen<StateType>& ray_gen, Scene& scene)
        : ray_gen_(ray_gen)
        , scene_(scene)
    {}

    template<typename Obj>
    void run_iteration(AtomicImage& out, Obj* integrator,
                       void (Obj::*process_shadow_rays)(RayQueue<StateType>&, AtomicImage&),
                       void (Obj::*process_primary_rays)(RayQueue<StateType>&, RayQueue<StateType>&, RayQueue<StateType>&, AtomicImage&),
                       SamplePixelFn sample_fn) {
        static_cast<Derived*>(this)->derived_run_iteration(out, integrator, process_shadow_rays, process_primary_rays, sample_fn);
    }

protected:
    RayGen<StateType>& ray_gen_;
    Scene& scene_;
};

}

#include "queue_scheduler_impl.h"
#include "tile_scheduler_impl.h"

#endif
