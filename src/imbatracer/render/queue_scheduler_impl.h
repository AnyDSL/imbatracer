#include <condition_variable>

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

enum QueueTag {
    QUEUE_EMPTY,
    QUEUE_IN_USE,
    QUEUE_READY_FOR_TRAVERSAL,
    QUEUE_READY_FOR_SHADOW_TRAVERSAL
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

    /// Checks if there are still any queues left that are not empty.
    bool has_nonempty() { return nonempty_count_ > 0; }

    size_t size() { return queues_.size(); }

private:
    std::vector<RayQueue<StateType>*> queues_;
    std::vector<std::atomic<QueueTag> > queue_flags_;

    std::atomic<size_t> nonempty_count_;
};

/// Uses a fixed number of queues, and multiple shading threads.
/// Traversal runs in the main thread and some other optimizations have been made for the GPU traversal.
/// Thus, the QueueScheduler should never be used with the CPU traversal.
template<typename StateType, int shadow_queue_count, int max_shadow_rays_per_hit>
class QueueScheduler : public RaySchedulerBase<QueueScheduler<StateType, shadow_queue_count, max_shadow_rays_per_hit>, StateType> {
    using BaseType = RaySchedulerBase<QueueScheduler<StateType, shadow_queue_count, max_shadow_rays_per_hit>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
    static constexpr int DEFAULT_QUEUE_SIZE = 65536;//1 << 16;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

    inline int primary_queue_count(RayGen<StateType>& ray_gen, int queue_size) {
        double size = ray_gen.width() * ray_gen.height() * ray_gen.num_samples() / static_cast<double>(queue_size);
        return std::ceil(size);
    }

public:
    QueueScheduler(RayGen<StateType>& ray_gen, Scene& scene, int queue_size = DEFAULT_QUEUE_SIZE)
        : BaseType(ray_gen, scene)
        , queue_pool_(queue_size, 2 * primary_queue_count(ray_gen, queue_size))
        , shadow_queue_pool_(queue_size * max_shadow_rays_per_hit,
            std::max(shadow_queue_count, 1 + primary_queue_count(ray_gen, queue_size)))
        , shadow_buffer_(queue_size * max_shadow_rays_per_hit * std::max(shadow_queue_count, 1 + primary_queue_count(ray_gen, queue_size)))
        , shadow_buffer_size_(queue_size * max_shadow_rays_per_hit)
    {
        // Initialize the GPU buffer
        RayQueue<StateType>::setup_device_buffer(shadow_buffer_size_);
    }

    ~QueueScheduler() {
        RayQueue<StateType>::release_device_buffer();
    }

    template<typename ShFunc, typename PrimFunc>
    void derived_run_iteration(AtomicImage& out,
                               ShFunc process_shadow_rays, PrimFunc process_primary_rays,
                               SamplePixelFn sample_fn) {
        ray_gen_.start_frame();

        fill_primary_queues(sample_fn);

        while (queue_pool_.has_nonempty() ||
               shadow_queue_pool_.has_nonempty()) {
            auto trav_q = queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_TRAVERSAL);
            if (!trav_q) {
                // There is no queue of primary rays ready for traversal
                process_shadow_queues(out, process_shadow_rays); // Find something to do.

                // Wait on the condition variable which signals that a shading thread is finished.

                continue; // busy wait for now
            }

            trav_q->traverse(scene_);

            auto q_out = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY); // We ensured that there will always be an output queue.

            auto q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            if (!q_out_shadow) {
                // If there is no empty shadow ray queue available, traverse the full ones.
                // Because the number of shadow queues is larger than the number of queues filled with
                // rays from the ray generation, there will always be at least one full queue ready for traversal,
                // if none of the queues are empty.
                process_shadow_queues(out, process_shadow_rays);

                // At least one queue will be empty now.
                q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            }

            shading_tasks_.run([this, process_primary_rays, trav_q, q_out, q_out_shadow, &out] () {
                process_primary_rays(*trav_q, *q_out, *q_out_shadow, out);

                trav_q->clear();
                queue_pool_.return_queue(trav_q, QUEUE_EMPTY);
                queue_pool_.return_queue(q_out, QUEUE_READY_FOR_TRAVERSAL);
                shadow_queue_pool_.return_queue(q_out_shadow, QUEUE_READY_FOR_SHADOW_TRAVERSAL);
            });
        }

        shading_tasks_.wait();
    }

private:
    RayQueuePool<StateType> queue_pool_;
    RayQueuePool<StateType> shadow_queue_pool_;

    RayQueue<StateType> shadow_buffer_;

    tbb::task_group shading_tasks_;

    std::condition_variable cv_;
    std::mutex cv_m_;
    int shading_done_;

    int shadow_buffer_size_;

    void fill_primary_queues(SamplePixelFn sample_fn) {
        while (!ray_gen_.is_empty()) {
            // We can be sure that there will always be a queue available.
            auto q = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            ray_gen_.fill_queue(*q, sample_fn);
            queue_pool_.return_queue(q, QUEUE_READY_FOR_TRAVERSAL);
        }
    }

    template<typename ShFunc>
    void process_shadow_queues(AtomicImage& out, ShFunc process_shadow_rays) {
        // Add all available shadow queues to the combined buffer.
        auto q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_SHADOW_TRAVERSAL);

        if (q_out_shadow) {
            q_out_shadow->traverse_occluded(scene_);
            process_shadow_rays(*q_out_shadow, out);
            shadow_queue_pool_.return_queue(q_out_shadow, QUEUE_EMPTY);
        }
        return;

        // static std::vector<QueueReference<StateType> > used_refs;
        // used_refs.reserve(shadow_queue_pool_.size());
        // used_refs.clear();

        // int total = 0;
        // while (q_out_shadow) {
        //     if (total + q_out_shadow->size() <= shadow_buffer_size_) {
        //         total += q_out_shadow->size();
        //         used_refs.push_back(q_out_shadow);
        //     } else {
        //         // We have enough rays in the buffer to utilize the GPU.
        //         shadow_queue_pool_.return_queue(q_out_shadow, QUEUE_READY_FOR_SHADOW_TRAVERSAL);
        //         break;
        //     }

        //     q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_SHADOW_TRAVERSAL);
        // }

        // RayQueue<StateType>::traverse_occluded_multi(used_refs.begin(), used_refs.end(), scene_);

        // for (auto& q_ref: used_refs) {
        //     process_shadow_rays(*q_ref, out);
        //     shadow_queue_pool_.return_queue(q_ref, QUEUE_EMPTY);
        // }
    }
};

} // namespace imba
