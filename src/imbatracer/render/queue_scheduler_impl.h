#include <condition_variable>
#include <mutex>

namespace imba {

template<typename StateType>
class RayQueuePool;

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

template<typename StateType>
class RayQueuePool {
public:
    RayQueuePool(size_t queue_size, size_t count)
        : queues_(count), queue_flags_(count)
    {
        for (auto iter = queue_flags_.begin(); iter != queue_flags_.end(); ++iter)
            iter->store(QUEUE_EMPTY);

        for (auto& q : queues_)
            q = new RayQueue<StateType>(queue_size);

        nonempty_count_ = 0;
    }

    ~RayQueuePool() {
        for (auto& q : queues_)
            delete q;
    }

    /// Finds the next queue that matches the given tag, sets its tag to QUEUE_IN_USE and returns it.
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

/// Uses a fixed number of queues, and multiple shading threads.
/// Traversal runs in the main thread and some other optimizations have been made for the GPU traversal.
/// Thus, the QueueScheduler should never be used with the CPU traversal.
template<typename StateType, int max_shadow_rays_per_hit>
class QueueScheduler : public RaySchedulerBase<QueueScheduler<StateType, max_shadow_rays_per_hit>, StateType> {
    using BaseType = RaySchedulerBase<QueueScheduler<StateType, max_shadow_rays_per_hit>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
    static constexpr int DEFAULT_QUEUE_SIZE = 1 << 16;
    static constexpr int DEFAULT_QUEUE_COUNT = 12;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

public:
    QueueScheduler(RayGen<StateType>& ray_gen,
                   Scene& scene,
                   int queue_size = DEFAULT_QUEUE_SIZE,
                   int queue_count = DEFAULT_QUEUE_COUNT)
        : BaseType(ray_gen, scene)
        , primary_queue_pool_(queue_size, queue_count)
        , shadow_queue_pool_(queue_size * max_shadow_rays_per_hit, 2 * queue_count / 3 + 1)
    {
        // Initialize the GPU buffer
        RayQueue<StateType>::setup_device_buffer(queue_size * max_shadow_rays_per_hit);
    }

    ~QueueScheduler() noexcept(true) {
        RayQueue<StateType>::release_device_buffer();
    }

    template<typename ShFunc, typename PrimFunc>
    void derived_run_iteration(AtomicImage& out,
                               ShFunc process_shadow_rays, PrimFunc process_primary_rays,
                               SamplePixelFn sample_fn) {
        ray_gen_.start_frame();

        done_shading_ = 0;
        while (!ray_gen_.is_empty() || primary_queue_pool_.nonempty_count()) {
            bool idle = true;

            // Traverse a shadow queue and process it in parallel
            auto q_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_TRAVERSAL);
            if (q_shadow) {
                idle = false;
                q_shadow->traverse_occluded(scene_.traversal_data());
                shading_tasks_.run([this, process_shadow_rays, q_shadow, &out] {
                    process_shadow_rays(*q_shadow, out);
                    shadow_queue_pool_.return_queue(q_shadow, QUEUE_EMPTY);
                });
            }

            // Traverse a primary ray queue
            auto q_primary = primary_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_TRAVERSAL);
            if (q_primary) {
                idle = false;
                q_primary->traverse(scene_.traversal_data());
            } else {
                q_primary = primary_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_SHADING);
            }

            // Try to shade a queue of rays
            auto q_shadow_out = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            auto q_out = primary_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            if (q_primary && q_shadow_out && q_out) {
                idle = false;
                shading_tasks_.run([this, process_primary_rays, &out, q_primary, q_out, q_shadow_out] () {
                    process_primary_rays(*q_primary, *q_out, *q_shadow_out, out);

                    primary_queue_pool_.return_queue(q_primary, QUEUE_EMPTY);
                    primary_queue_pool_.return_queue(q_out, QUEUE_READY_FOR_TRAVERSAL);

                    shadow_queue_pool_.return_queue(q_shadow_out, QUEUE_READY_FOR_TRAVERSAL);

                    // Notify the scheduler that one shading process is done
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        done_shading_++;
                    }
                    cv_.notify_all();
                });
            } else {
                // We cannot shade the rays in the queue, so we postpone them for the next iteration
                if (q_primary)    primary_queue_pool_.return_queue(q_primary, QUEUE_READY_FOR_SHADING);
                if (q_out)        primary_queue_pool_.return_queue(q_out, QUEUE_EMPTY);

                if (q_shadow_out) shadow_queue_pool_.return_queue(q_shadow_out, QUEUE_EMPTY);
            }

            // Try to generate rays in empty queues
            int n = primary_queue_pool_.nonempty_count();
            while (!ray_gen_.is_empty() && n < primary_queue_pool_.size() / 2) {
                auto q_empty = primary_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
                if (!q_empty) break;
                idle = false;
                ray_gen_.fill_queue(*q_empty, sample_fn);
                primary_queue_pool_.return_queue(q_empty, QUEUE_READY_FOR_TRAVERSAL);
                n++;
            }

            // Fill queues which are not completely filled with new rays (threshold is 75%)
            while (!ray_gen_.is_empty()) {
                auto q_regen = primary_queue_pool_.claim_queue_for_regen(QUEUE_READY_FOR_TRAVERSAL, 0.75f);
                if (!q_regen) break;
                idle = false;
                ray_gen_.fill_queue(*q_regen, sample_fn);
                primary_queue_pool_.return_queue(q_regen, QUEUE_READY_FOR_TRAVERSAL);
            }

            // If nothing happened this iteration, wait for the next shading task
            if (idle) {
                std::unique_lock<std::mutex> lock(mutex_);
                while (done_shading_ <= 0) cv_.wait(lock);
                done_shading_--;
            }
        }

        shading_tasks_.wait();
    }

private:
    RayQueuePool<StateType> primary_queue_pool_;
    RayQueuePool<StateType> shadow_queue_pool_;
    tbb::task_group shading_tasks_;

    std::condition_variable cv_;
    std::mutex mutex_;
    int done_shading_;
};

} // namespace imba
