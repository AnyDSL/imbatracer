#ifndef IMBA_QUEUE_POOL_H
#define IMBA_QUEUE_POOL_H

#include "imbatracer/render/scheduling/ray_scheduler.h"

namespace imba {

template <typename StateType>
class RayQueuePool;

template <typename StateType>
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

    void release() {
        q_ = nullptr;
        idx_ = 0;
    }

private:
    mutable RayQueue<StateType>* q_;
    size_t idx_;
};

enum QueueTag : int {
    QUEUE_EMPTY,
    QUEUE_IN_USE,
    QUEUE_READY_FOR_SHADING,
    QUEUE_READY_FOR_TRAVERSAL
};

template <typename StateType>
class RayQueuePool {
public:
    RayQueuePool(size_t queue_size, size_t count, bool gpu_traversal = true)
        : queues_(count), queue_flags_(count)
    {
        for (auto iter = queue_flags_.begin(); iter != queue_flags_.end(); ++iter)
            iter->store(QUEUE_EMPTY);

        for (auto& q : queues_)
            q = new RayQueue<StateType>(queue_size, gpu_traversal);

        nonempty_count_ = 0;
    }

    ~RayQueuePool() {
        for (auto& q : queues_)
            delete q;
    }

    /// Finds the next queue that matches the given tag, sets its tag to QUEUE_IN_USE and returns it.
    /// \returns Either a valid reference to a queue matching the tag, or a reference that equals nullptr
    QueueReference<StateType> claim_queue_with_tag(QueueTag tag) {
        for (size_t i = 0; i < queues_.size(); ++i) {
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

    /// Finds the next queue that matches the given tag and that,
    /// has a smaller fill factor than the given value,
    /// sets its tag to QUEUE_IN_USE and returns it.
    QueueReference<StateType> claim_queue_for_regen(QueueTag tag, float fill_factor) {
        for (size_t i = 0; i < queues_.size(); ++i) {
            QueueTag expected = tag;
            if (queue_flags_[i].compare_exchange_strong(expected, QUEUE_IN_USE)) {
                if (queues_[i]->size() < queues_[i]->capacity() * fill_factor) {
                    // We found a matching queue.
                    if (tag == QUEUE_EMPTY)
                        nonempty_count_++;
                    return QueueReference<StateType>(*queues_[i], i);
                }

                // Restore the tag
                queue_flags_[i].store(tag);
            }
        }

        return QueueReference<StateType>();
    }

    void return_queue(QueueReference<StateType> ref, QueueTag new_tag) {
        // Tag all returned queues that are empty as QUEUE_EMPTY.
        if (ref->size() <= 0)
            new_tag = QUEUE_EMPTY;

        // Clear all queues that were returned with tag QUEUE_EMPTY.
        if (new_tag == QUEUE_EMPTY) {
            ref->clear();
            nonempty_count_--;
        }

        queue_flags_[ref.index()] = new_tag;
    }

    /// Checks if there are still any non-empty queues left.
    bool has_nonempty() const { return nonempty_count_ > 0; }

    int nonempty_count() const { return nonempty_count_; }

    size_t size() { return queues_.size(); }

private:
    std::vector<RayQueue<StateType>*> queues_;
    std::vector<std::atomic<QueueTag> > queue_flags_;

    std::atomic<size_t> nonempty_count_;
};

} // namespace imba

#endif // IMBA_QUEUE_POOL_H