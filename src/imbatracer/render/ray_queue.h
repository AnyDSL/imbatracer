#ifndef IMBA_RAY_QUEUE_H
#define IMBA_RAY_QUEUE_H

#include "traversal.h"
#include "thorin_mem.h"
#include "scene.h"

#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <atomic>
#include <mutex>

namespace imba {

/// Base class for storing the current state associated with a ray.
struct RayState {
    int pixel_id;
    int sample_id;

    RNG rng;
};

// Allow running multiple traversal instances at the same time, if traversal is running on the CPU.
#ifdef GPU_TRAVERSAL

static std::mutex traversal_mutex;
static const int TRAVERSAL_BLOCK_SIZE = 64;

#else

static const int TRAVERSAL_BLOCK_SIZE = 8;

#endif

/// Stores a set of rays for traversal along with their state.
template <typename StateType>
class RayQueue {
    inline int align(int v) { return v + TRAVERSAL_BLOCK_SIZE - v % TRAVERSAL_BLOCK_SIZE; }
public:
    RayQueue() { }

    RayQueue(int capacity)
        : ray_buffer_(align(capacity))
        , hit_buffer_(align(capacity))
        , state_buffer_(align(capacity))
        , last_(-1)
    {
    }

    RayQueue(const RayQueue<StateType>&) = delete;
    RayQueue& operator= (const RayQueue<StateType>&) = delete;

    RayQueue(RayQueue<StateType>&& rhs)
        : ray_buffer_(std::move(rhs.ray_buffer_)),
          hit_buffer_(std::move(rhs.hit_buffer_)),
          state_buffer_(std::move(rhs.state_buffer_)),
          last_(rhs.last_.load())
    {}

    RayQueue& operator= (RayQueue<StateType>&& rhs) {
        ray_buffer_ = std::move(rhs.ray_buffer_);
        hit_buffer_ = std::move(rhs.hit_buffer_);
        state_buffer_ = std::move(rhs.state_buffer_);
        last_ = rhs.last_.load();

        return *this;
    }

    int size() const { return last_ + 1; }
    int capacity() const { return state_buffer_.size(); }

    ::Ray* rays() {
        return ray_buffer_.host_data();
    }

    StateType* states() {
        return state_buffer_.data();
    }

    ::Hit* hits() {
        return hit_buffer_.host_data();
    }

    void clear() {
        last_ = -1;
    }

    /// Adds a single secondary or shadow ray to the queue. Thread-safe
    void push(const Ray& ray, const StateType& state) {
        int id = ++last_; // atomic inc. of last_

        assert(id < ray_buffer_.size() && "ray queue full");

        ray_buffer_[id] = ray;
        state_buffer_[id] = state;
    }

    /// Adds a set of camera rays to the queue. Thread-safe
    template<typename RayIter, typename StateIter>
    void push(RayIter rays_begin, RayIter rays_end, StateIter states_begin, StateIter states_end) {
        // Calculate the position at which the rays will be inserted.
        int count = rays_end - rays_begin;
        int end_idx = last_ += count; // atomic add to last_
        int start_idx = end_idx - (count - 1);\

        assert(end_idx < ray_buffer_.size() && "ray queue full");

        // Copy ray and state data.
        std::copy(rays_begin, rays_end, ray_buffer_.begin() + start_idx);
        std::copy(states_begin, states_end, state_buffer_.begin() + start_idx);
    }

    /// Traverses the acceleration structure with the rays currently inside the queue.
    void traverse(Scene& scene) {
        assert(size() != 0);

        int count = size();
        if (count % TRAVERSAL_BLOCK_SIZE != 0) {
            count = count + TRAVERSAL_BLOCK_SIZE - count % TRAVERSAL_BLOCK_SIZE;
        }

        ray_buffer_.upload(size());

        {
#ifndef CPU_TRAVERSAL
            std::lock_guard<std::mutex> lock(traversal_mutex);
#endif
            TRAVERSAL_INTERSECT(scene.nodes.device_data(),
                                scene.tris.device_data(),
                                ray_buffer_.device_data(),
                                hit_buffer_.device_data(),
                                scene.indices.device_data(),
                                scene.texcoords.device_data(),
                                scene.masks.device_data(),
                                scene.mask_buffer.device_data(),
                                count);
        }

        hit_buffer_.download(size());
    }

    void traverse_occluded(Scene& scene) {
        assert(size() != 0);

        int count = size();
        if (count % TRAVERSAL_BLOCK_SIZE != 0) {
            count = count + TRAVERSAL_BLOCK_SIZE - count % TRAVERSAL_BLOCK_SIZE;
        }

        ray_buffer_.upload(size());

        {
#ifndef CPU_TRAVERSAL
            std::lock_guard<std::mutex> lock(traversal_mutex);
#endif
            TRAVERSAL_OCCLUDED(scene.nodes.device_data(),
                               scene.tris.device_data(),
                               ray_buffer_.device_data(),
                               hit_buffer_.device_data(),
                               scene.indices.device_data(),
                               scene.texcoords.device_data(),
                               scene.masks.device_data(),
                               scene.mask_buffer.device_data(),
                               count);
        }

        hit_buffer_.download(size());
    }

private:
    ThorinArray<::Ray> ray_buffer_;
    ThorinArray<::Hit> hit_buffer_;
    std::vector<StateType> state_buffer_;

    std::atomic<int> last_;
};

}

#endif
