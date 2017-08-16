#ifndef IMBA_RAYQUEUE_SCHEDULER_H
#define IMBA_RAYQUEUE_SCHEDULER_H

#include "imbatracer/render/scheduling/queue_pool.h"

#include <functional>
#include <condition_variable>
#include <mutex>

namespace imba {

template <typename StateType>
class QueueScheduler {
    const bool gpu;

public:
    using ShadeFn      = typename std::function<void (Ray&, Hit&, StateType&)>;
    using ShadeEmptyFn = typename std::function<void (Ray&, StateType&)>;

    QueueScheduler(int capacity, int traversal_chunk_size, int traversal_thread_count, bool gpu)
        : trav_chunk_sz_(traversal_chunk_size)
        , q_(capacity, gpu)
        , gpu(gpu)
        , workers_(traversal_thread_count)
        , busy_workers_(0)
        , done_(false)
        , started_(false)
        , cur_chunk_(0)
        , flush_(false)
    {
        resize(capacity);

        // Launch the traversal worker threads
        int i = 0;
        for (auto& t : workers_) {
            t = std::thread([this, i]() { traversal_worker(i); });
            i++;
        }
    }

    ~QueueScheduler() {
        // Notify the workers to terminate and wait for them
        { std::unique_lock<std::mutex> lock(mutex_);
            done_ = true;
        }
        cv_work_.notify_all();

        for (auto& t : workers_)
            t.join();
    }

    /// Changes the maximum total number of rays to the given number.
    /// Calling this function in parallel with \see{push} results in undef. behavior.
    void resize(int sz) {
        if (sz > q_.capacity())
            q_.resize(sz);
    }

    /// Has to be called before doing any work on a batch of rays.
    void start(const Scene* s, bool shadow_only, ShadeFn hit, ShadeEmptyFn miss) {
        { std::unique_lock<std::mutex> lock(mutex_);
            cur_chunk_        = 0;
            flush_            = false;
            started_          = true;
            busy_workers_     = 0;
            hit_callback_     = hit;
            miss_callback_    = miss;
            shadow_only_      = shadow_only;
            scene_            = s;
            q_.clear();
        }
        cv_work_.notify_all();
    }

    /// Adds a new ray to the queue. SLOW! Try to use the array version whenever possible.
    /// If sufficient rays are available, traversal is called asynchronously.
    void push(const Ray& r, const StateType& s) {
        // TODO make this lock free by using atomics

        std::unique_lock<std::mutex> lock(mutex_);

        int idx = q_.push(r, s);

        // If this is the last ray in a chunk, notify the traversal workers.
        if ((idx + 1) % trav_chunk_sz_ == 0 && idx > 0)
            cv_work_.notify_all();
    }

    /// Adds a set of rays to the queue.
    /// Automatically triggers traversal / shading if a sufficient number of rays is available.
    void push(Ray* rays, StateType* states, int count) {
        // TODO make this lock free by using atomics

        std::unique_lock<std::mutex> lock(mutex_);

        int idx = 0;
        for (int i = 0; i < count; ++i) {
            idx = q_.push(rays[i], states[i]);

            if ((idx + 1) % trav_chunk_sz_ == 0 && idx > 0)
                cv_work_.notify_all();
        }
    }

    /// Traverses and shades all remaining rays in the queue. Blocks until done.
    void flush() {
        std::unique_lock<std::mutex> lock(mutex_);
        flush_   = true;
        started_ = false;

        // Wait for all chunks to be processed
        while (cur_chunk_ * trav_chunk_sz_ < q_.size())
            cv_done_.wait(lock);

        cv_work_.notify_all();

        while (busy_workers_ > 0)
            cv_done_.wait(lock);
    }

private:
    RayQueue<StateType> q_;
    int trav_chunk_sz_;
    int cur_chunk_;

    std::condition_variable cv_work_;
    std::condition_variable cv_done_;
    std::mutex mutex_;
    bool done_;
    bool flush_;
    bool started_;

    int busy_workers_;
    std::vector<std::thread> workers_;

    ShadeFn hit_callback_;
    ShadeEmptyFn miss_callback_;
    bool shadow_only_;

    const Scene* scene_;

    void traversal_worker(int id) {
        std::unique_lock<std::mutex> lock(mutex_);

        bool notified_done  = false;
        bool notified_start = false;
        while (!done_) {
            // Wait for a job to start
            while (!started_ && !done_ && cur_chunk_ * trav_chunk_sz_ >= q_.size()) {
                if (!notified_done) {
                    busy_workers_--;
                    cv_done_.notify_all();
                    notified_done  = true;
                    notified_start = false;
                }
                cv_work_.wait(lock);
            }
            if (done_) break;

            notified_done = false;
            if (!notified_start) {
                busy_workers_++;
                cv_done_.notify_all();
                notified_start = true;
            }

            // Call dibs on a chunk and wait for it to be generated.
            int chunk = ++cur_chunk_;
            if (chunk > q_.capacity() / trav_chunk_sz_) {
                // This chunk does not exist -> we are done, wait for flush
                busy_workers_--;
                notified_done  = true;
                notified_start = false;
                cv_done_.notify_all();
                while (!flush_ && !done_) cv_work_.wait(lock);
                continue;
            }

            while (chunk * trav_chunk_sz_ > q_.size() && !flush_ && !done_)
                cv_work_.wait(lock);

            if (done_) break;

            int begin = (chunk - 1) * trav_chunk_sz_;
            int end = std::min(q_.size(), begin + trav_chunk_sz_);

            // This chunk was never generated
            if (begin >= end) {
                busy_workers_--;
                notified_done  = true;
                notified_start = false;
                cv_done_.notify_all();
                while (!flush_ && !done_) cv_work_.wait(lock);
                continue;
            };

            // Traverse and shade the chunk in parallel
            lock.unlock();
            traverse(begin, end);
            shade(begin, end);
            lock.lock();
        }
    }

    void traverse(int begin, int end) {
        if (shadow_only_) {
            if (gpu) q_.traverse_occluded_gpu(scene_->traversal_data_gpu(), begin, end);
            else     q_.traverse_occluded_cpu(scene_->traversal_data_cpu(), begin, end);
        } else {
            if (gpu) q_.traverse_gpu(scene_->traversal_data_gpu(), begin, end);
            else     q_.traverse_cpu(scene_->traversal_data_cpu(), begin, end);
        }
    }

    void shade(int begin, int end) {
        const int hit_count = q_.compact_hits(begin, end);

        if (miss_callback_) {
            tbb::parallel_for(tbb::blocked_range<int>(begin + hit_count, end),
            [&] (const tbb::blocked_range<int>& range)
            {
                for (auto i = range.begin(); i != range.end(); ++i) {
                    miss_callback_(q_.ray(i), q_.state(i));
                }
            });
        }

        if (hit_callback_) {
            q_.sort_by_material([this](const Hit& hit){ return scene_->mat_id(hit); },
                                scene_->material_count(), hit_count, begin);

            tbb::parallel_for(tbb::blocked_range<int>(begin, begin + hit_count),
            [&] (const tbb::blocked_range<int>& range)
            {
                for (auto i = range.begin(); i != range.end(); ++i) {
                    hit_callback_(q_.ray(i), q_.hit(i), q_.state(i));
                }
            });
        }
    }
};

}

#endif // IMBA_RAYQUEUE_SCHEDULER_H