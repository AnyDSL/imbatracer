#ifndef IMBA_RAY_QUEUE_H
#define IMBA_RAY_QUEUE_H

#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <atomic>
#include <mutex>
#include <memory>

#include <thorin_runtime.hpp>
#include <traversal.h>

#include "random.h"

namespace imba {

/// Base class for storing the current state associated with a ray.
struct RayState {
    int pixel_id;
    int sample_id;

    RNG rng;
};

#ifdef GPU_TRAVERSAL

#define TRAVERSAL_DEVICE    thorin::Device(0)
#define TRAVERSAL_PLATFORM  thorin::Platform::CUDA
#define TRAVERSAL_INTERSECT intersect_gpu_masked_instanced
#define TRAVERSAL_OCCLUDED  occluded_gpu_masked_instanced

// Do not allow running multiple traversal instances at the same time on the GPU.
static std::mutex traversal_mutex;
static constexpr int traversal_block_size() { return 64; }

#else

static constexpr int traversal_block_size() { return 8; }

#define TRAVERSAL_DEVICE    thorin::Device(0)
#define TRAVERSAL_PLATFORM  thorin::Platform::HOST
#define TRAVERSAL_INTERSECT intersect_cpu_masked_instanced
#define TRAVERSAL_OCCLUDED  occluded_cpu_masked_instanced

#endif

/// Structure that contains the traversal data, such as the BVH nodes or opacity masks.
struct TraversalData {
    thorin::Array<::Node> nodes;
    thorin::Array<::InstanceNode> instances;
    thorin::Array<::Vec4> tris;
    thorin::Array<::Vec2> texcoords;
    thorin::Array<int> indices;
    thorin::Array<::TransparencyMask> masks;
    thorin::Array<char> mask_buffer;
};

/// Stores a set of rays for traversal along with their state.
template <typename StateType>
class RayQueue {
    static int align(int v) { return v % traversal_block_size() == 0 ? v : v + traversal_block_size() - v % traversal_block_size(); }

public:
    RayQueue() { }

    RayQueue(int capacity)
        : ray_buffer_(align(capacity))
        , hit_buffer_(align(capacity))
        , state_buffer_(align(capacity))
        , last_(-1)
    {
        // Initializing ray and hit buffer memory to zero helps with traversal debugging.
        memset(ray_buffer_.data(), 0, sizeof(Ray) * align(capacity));
        memset(hit_buffer_.data(), 0, sizeof(Hit) * align(capacity));
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
        return ray_buffer_.data();
    }

    StateType* states() {
        return state_buffer_.data();
    }

    ::Hit* hits() {
        return hit_buffer_.data();
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
        int start_idx = end_idx - (count - 1);

        assert(end_idx < ray_buffer_.size() && "ray queue full");

        // Copy ray and state data.
        std::copy(rays_begin, rays_end, ray_buffer_.begin() + start_idx);
        std::copy(states_begin, states_end, state_buffer_.begin() + start_idx);
    }

    // Appends the rays and state data from another queue to this queue. Hits are not copied.
    void append(const RayQueue<StateType>& other) {
        int count = other.size();
        int end_idx = last_ += count; // atomic add to last_
        int start_idx = end_idx - (count - 1);

        assert(end_idx < ray_buffer_.size() && "ray queue full");

        // Copy ray and state data.
        std::copy(other.ray_buffer_.begin(), other.ray_buffer_.end(), ray_buffer_.begin() + start_idx);
        std::copy(other.state_buffer_.begin(), other.state_buffer_.end(), state_buffer_.begin() + start_idx);
    }

    /// Traverses the acceleration structure with the rays currently inside the queue.
    void traverse(const TraversalData& c_data) {
        assert(size() != 0);

        int count = align(size());
        TraversalData& data = const_cast<TraversalData&>(c_data);

#ifdef GPU_TRAVERSAL
        {
            std::lock_guard<std::mutex> lock(traversal_mutex);

            thorin::copy(ray_buffer_, *device_ray_buffer.get(), size());

            TRAVERSAL_INTERSECT(data.nodes.data(),
                                data.instances.data(),
                                data.tris.data(),
                                device_ray_buffer->data(),
                                device_hit_buffer->data(),
                                data.indices.data(),
                                data.texcoords.data(),
                                data.masks.data(),
                                data.mask_buffer.data(),
                                count);

            thorin::copy(*device_hit_buffer.get(), hit_buffer_, size());
        }
#else
        TRAVERSAL_INTERSECT(data.nodes.data(),
                            data.instances.data(),
                            data.tris.data(),
                            ray_buffer_.data(),
                            hit_buffer_.data(),
                            data.indices.data(),
                            data.texcoords.data(),
                            data.masks.data(),
                            data.mask_buffer.data(),
                            count);
#endif
    }

    void traverse_occluded(const TraversalData& c_data) {
        assert(size() != 0);

        int count = align(size());
        TraversalData& data = const_cast<TraversalData&>(c_data);

#ifdef GPU_TRAVERSAL
        {
            std::lock_guard<std::mutex> lock(traversal_mutex);

            thorin::copy(ray_buffer_, *device_ray_buffer.get(), size());

            TRAVERSAL_OCCLUDED(data.nodes.data(),
                               data.instances.data(),
                               data.tris.data(),
                               device_ray_buffer->data(),
                               device_hit_buffer->data(),
                               data.indices.data(),
                               data.texcoords.data(),
                               data.masks.data(),
                               data.mask_buffer.data(),
                               count);

            thorin::copy(*device_hit_buffer.get(), hit_buffer_, size());
        }
#else
        TRAVERSAL_OCCLUDED(data.nodes.data(),
                           data.instances.data(),
                           data.tris.data(),
                           ray_buffer_.data(),
                           hit_buffer_.data(),
                           data.indices.data(),
                           data.texcoords.data(),
                           data.masks.data(),
                           data.mask_buffer.data(),
                           count);
#endif
    }

#ifdef GPU_TRAVERSAL
private:
    // Keep only one shared buffer on the device to reduce memory usage.
    static std::unique_ptr<thorin::Array<Ray> > device_ray_buffer;
    static std::unique_ptr<thorin::Array<Hit> > device_hit_buffer;
    static size_t device_buffer_size;
public:
    static void setup_device_buffer(size_t max_count) {
        device_ray_buffer.reset(new thorin::Array<Ray>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, align(max_count)));
        device_hit_buffer.reset(new thorin::Array<Hit>(TRAVERSAL_PLATFORM, TRAVERSAL_DEVICE, align(max_count)));
        device_buffer_size = max_count;
    }

    static void release_device_buffer() {
        device_hit_buffer.reset(nullptr);
        device_ray_buffer.reset(nullptr);
    }

#else
public:
    static void setup_device_buffer(size_t max_count) {}
    static void release_device_buffer() {}
#endif

private:
    thorin::Array<Ray> ray_buffer_;
    thorin::Array<Hit> hit_buffer_;

    std::vector<StateType> state_buffer_;

    std::atomic<int> last_;
};

#ifdef GPU_TRAVERSAL

template<typename StateType>
std::unique_ptr<thorin::Array<Ray> > RayQueue<StateType>::device_ray_buffer;
template<typename StateType>
std::unique_ptr<thorin::Array<Hit> > RayQueue<StateType>::device_hit_buffer;
template<typename StateType>
size_t RayQueue<StateType>::device_buffer_size;

#endif

} // namespace imba

#endif // IMBA_RAY_QUEUE
