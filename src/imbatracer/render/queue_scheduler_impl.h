
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

        if (new_tag == QUEUE_EMPTY)
            nonempty_count_--;

        queue_flags_[ref.index()] = new_tag;
    }

    /// Checks if there are still any queues left that are not empty.
    bool has_nonempty() { return nonempty_count_ > 0; }

private:
    std::vector<RayQueue<StateType>*> queues_;
    std::vector<std::atomic<QueueTag> > queue_flags_;

    std::atomic<size_t> nonempty_count_;
};

/// Uses a fixed number of queues, and multiple shading threads.
/// Traversal runs in the main thread. Thus, the scheduler is most beneficial for the GPU traversal.
template<typename StateType, int shadow_queue_count, int max_shadow_rays_per_hit>
class QueueScheduler : public RaySchedulerBase<QueueScheduler<StateType, shadow_queue_count, max_shadow_rays_per_hit>, StateType> {
    using BaseType = RaySchedulerBase<QueueScheduler<StateType, shadow_queue_count, max_shadow_rays_per_hit>, StateType>;
    using SamplePixelFn = typename RayGen<StateType>::SamplePixelFn;
    static constexpr int DEFAULT_QUEUE_SIZE = 1 << 16;

protected:
    using BaseType::ray_gen_;
    using BaseType::scene_;

public:
    QueueScheduler(RayGen<StateType>& ray_gen, Scene& scene, int queue_size = DEFAULT_QUEUE_SIZE)
        : BaseType(ray_gen, scene)
        , queue_pool_(queue_size, (2 * ray_gen.width() * ray_gen.height() * ray_gen.num_samples()) / queue_size)
        , shadow_queue_pool_(queue_size * max_shadow_rays_per_hit, shadow_queue_count)
    {
        // Initialize the GPU buffer
        RayQueue<StateType>::setup_device_buffer(queue_size * max_shadow_rays_per_hit * shadow_queue_count);
    }

    ~QueueScheduler() {
        RayQueue<StateType>::release_device_buffer();
    }

    template<typename ShFunc, typename PrimFunc>
    void derived_run_iteration(AtomicImage& out,
                               ShFunc process_shadow_rays, PrimFunc process_primary_rays,
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

                process_shadow_rays(*q_ref, out);

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
            while (!q_out) {
                // All queues are currently occupied as input or output of shading tasks.
                shading_tasks_.wait();
                q_out = queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            }

            auto q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
            while (!q_out_shadow) {
                q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_READY_FOR_SHADOW_TRAVERSAL);
                if (q_out_shadow) { // A shadow ray queue is ready for traversal.
                    q_out_shadow->traverse_occluded(scene_);
                    process_shadow_rays(*q_out_shadow, out);
                    q_out_shadow->clear();
                    // The queue is now empty and can be used.
                } else {
                    shading_tasks_.wait();
                    // Try to get an empty shadow queue again.
                    q_out_shadow = shadow_queue_pool_.claim_queue_with_tag(QUEUE_EMPTY);
                }
            }

            shading_tasks_.run([this, process_primary_rays, q_ref, q_out, q_out_shadow, &out] () {
                process_primary_rays(*q_ref, *q_out, *q_out_shadow, out);

                q_ref->clear();
                queue_pool_.return_queue(q_ref, QUEUE_EMPTY);
                queue_pool_.return_queue(q_out, QUEUE_READY_FOR_TRAVERSAL);
                shadow_queue_pool_.return_queue(q_out_shadow, QUEUE_READY_FOR_SHADOW_TRAVERSAL);
            });
        }

        shading_tasks_.wait();
    }

private:
    RayQueuePool<StateType> queue_pool_;
    RayQueuePool<StateType> shadow_queue_pool_;

    tbb::task_group shading_tasks_;

    void fill_primary_queues() {

    }

    void process_shadow_queues() {

    }
};

} // namespace imba
