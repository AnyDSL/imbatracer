#ifndef IMBA_RAY_QUEUE_H
#define IMBA_RAY_QUEUE_H

#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <atomic>
#include <mutex>
#include <memory>

#define TBB_USE_EXCEPTIONS 0
#include <tbb/parallel_sort.h>
#include <anydsl_runtime.hpp>

#include "imbatracer/core/traversal_interface.h"
#include "imbatracer/render/random.h"

namespace imba {

/// State associated with a ray.
struct RayState {
    union {
        int pixel_id;
        int light_id;
    };

    union {
        int sample_id;
        int ray_id;
    };

    RNG rng;
};

/// State associated with a shadow ray
struct ShadowState {
    int pixel_id;
    rgb throughput;
};

/// Structure that contains the traversal data, such as the BVH nodes or opacity masks.
template <typename Node>
struct TraversalData {
    int root;
    anydsl::Array<Node> nodes;
    anydsl::Array<InstanceNode> instances;
    anydsl::Array<Vec4> tris;
};

/// Stores a set of rays for traversal along with their state.
template <typename StateType>
class RayQueue {
    static int align_cpu(int v) { return v %  8 == 0 ? v : v +  8 - v %  8; }
    static int align_gpu(int v) { return v % 64 == 0 ? v : v + 64 - v % 64; }
    static int align(int v) { return std::max(align_cpu(v), align_gpu(v)); }

public:
    RayQueue() { }

    RayQueue(int capacity, bool gpu_buffers)
        : last_(-1)
        , gpu_buffers_(gpu_buffers)
    {
        resize(capacity);
    }

    RayQueue(const RayQueue<StateType>&) = delete;
    RayQueue& operator= (const RayQueue<StateType>&) = delete;

    RayQueue(RayQueue<StateType>&& rhs)
        : ray_buffer_(std::move(rhs.ray_buffer_))
        , hit_buffer_(std::move(rhs.hit_buffer_))
        , state_buffer_(std::move(rhs.state_buffer_))
        , sorted_indices_(std::move(rhs.sorted_indices_))
        , last_(rhs.last_.load())
    {}

    RayQueue& operator= (RayQueue<StateType>&& rhs) {
        ray_buffer_ = std::move(rhs.ray_buffer_);
        hit_buffer_ = std::move(rhs.hit_buffer_);
        state_buffer_ = std::move(rhs.state_buffer_);
        sorted_indices_ = std::move(rhs.sorted_indices_);
        last_ = rhs.last_.load();

        return *this;
    }

    int size() const { return last_ + 1; }
    int capacity() const { return state_buffer_.size(); }

    void resize(int c) {
        ray_buffer_    = anydsl::Array<Ray>    (align(c));
        hit_buffer_    = anydsl::Array<Hit>    (align(c));
        state_buffer_  = std::vector<StateType>(align(c));
        sorted_indices_= std::vector<int>      (align(c));

        // Create buffers on the GPU if necessary.
        if (gpu_buffers_) {
            dev_ray_buffer_ = anydsl::Array<Ray>(anydsl::Platform::Cuda, anydsl::Device(0), align_gpu(c));
            dev_hit_buffer_ = anydsl::Array<Hit>(anydsl::Platform::Cuda, anydsl::Device(0), align_gpu(c));
        }
    }

    // Shrinks the queue to the given size.
    void shrink(int size) { last_ = size - 1; }

    Ray* rays() { return ray_buffer_.data(); }
    StateType* states() { return state_buffer_.data(); }
    Hit* hits() { return hit_buffer_.data(); }

    Ray& ray(int idx) { return ray_buffer_[sorted_indices_[idx]]; }
    Hit& hit(int idx) { return hit_buffer_[sorted_indices_[idx]]; }
    StateType& state(int idx) { return state_buffer_[sorted_indices_[idx]]; }

    void clear() {
        last_ = -1;
    }

    /// Adds a single secondary or shadow ray to the queue. Thread-safe
    /// \returns the index at which the ray was inserted.
    int push(const Ray& ray, const StateType& state) {
        int id = ++last_; // atomic inc. of last_

        assert(id < ray_buffer_.size() && "Attempted a push to a full ray queue.");

        ray_buffer_[id] = ray;
        state_buffer_[id] = state;

        return id;
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

    /// Compact the queue by moving all rays that hit something (and their associated states and hits) to the front.
    inline int compact_hits(int begin = 0, int end = -1) {
        if (end < 0) end = size();
        auto hits   = this->hits();
        auto states = this->states();
        auto rays   = this->rays();

        int last_empty = -1;
        for (int i = begin; i < end; ++i) {
            if (hits[i].tri_id < 0 && last_empty == -1) {
                last_empty = i;
            } else if (hits[i].tri_id >= 0 && last_empty != -1) {
                std::swap(hits[last_empty],   hits[i]);
                std::swap(states[last_empty], states[i]);
                std::swap(rays[last_empty],   rays[i]);
                last_empty++;
            }
        }

        for (int i = begin; i < end; ++i)
            sorted_indices_[i] = i;

        if (last_empty != -1)
            return last_empty - begin;
        else
            return end - begin;
    }

    /// Compacts the queue by moving all continued rays to the front. Does not move the hits.
    inline void compact_rays() {
        auto states = this->states();
        auto rays   = this->rays();

        int last_empty = -1;
        for (int i = 0; i < size(); ++i) {
            if (states[i].pixel_id < 0 && last_empty == -1) {
                last_empty = i;
            } else if (states[i].pixel_id >= 0 && last_empty != -1) {
                states[last_empty] = states[i];
                rays[last_empty]   = rays[i];
                last_empty++;
            }
        }

        // If at least one empty ray was replaced, shrink the queue.
        // last_empty corresponds to the new queue size.
        if (last_empty != -1)
            shrink(last_empty);
    }

    typedef std::function<int (const Hit&)> GetMatIDFn;

    inline void sort_by_material(GetMatIDFn get_mat_id, int num_mats, int count, int begin = 0) {
        // TODO / FIXME: sorting for new scheduler as part of traversal job, or after all hits are known?

        // thread_local std::vector<std::atomic<int> > matcount(num_mats);

        // if (matcount.size() < num_mats)
        //     matcount = std::vector<std::atomic<int> >(num_mats);
        // std::fill(matcount.begin(), matcount.end(), 0);

        // // Count the number of hit points per material.
        // tbb::parallel_for(tbb::blocked_range<int>(begin, begin + count),
        //     [&] (const tbb::blocked_range<int>& range)
        // {
        //     for (auto i = range.begin(); i != range.end(); ++i) {
        //         const int mat_id = get_mat_id(hit_buffer_[i]);
        //         ray_buffer_[i].dir.w = int_as_float(mat_id);
        //         matcount[mat_id]++;
        //     }
        // });

        // // Compute the starting index of every bin.
        // int accum = 0;
        // for (int i = 0; i < num_mats; ++i) {
        //     const int tmp = matcount[i];
        //     matcount[i] = accum;
        //     accum += tmp;
        // }

        // // Distribute the indices according to their material ids.
        // tbb::parallel_for(tbb::blocked_range<int>(begin, begin + count),
        //     [&] (const tbb::blocked_range<int>& range)
        // {
        //     for (auto i = range.begin(); i != range.end(); ++i) {
        //         const int mat = float_as_int(ray_buffer_[i].dir.w);
        //         sorted_indices_[begin + matcount[mat]++] = i;
        //     }
        // });
    }

    /// Traverses all rays currently in the queue on the CPU.
    void traverse_cpu(const TraversalData<traversal_cpu::Node>& c_data, int begin = 0, int end = -1) {
        if (end < 0) end = size(); // TODO: adapt for version with offsets
        // assert(size() != 0);

        // int count = align_cpu(size());
        // auto& data = const_cast<TraversalData<traversal_cpu::Node>&>(c_data);
        // auto nodes = reinterpret_cast<traversal_cpu::Node*>(data.nodes.data());

        // traversal_cpu::intersect_cpu_instanced(
        //     data.root,
        //     nodes,
        //     data.instances.data(),
        //     data.tris.data(),
        //     ray_buffer_.data(),
        //     hit_buffer_.data(),
        //     count);
    }

    // Traverses all rays currently in the queue on the GPU.
    void traverse_gpu(const TraversalData<traversal_gpu::Node>& c_data, int begin = 0, int end = -1) {
        if (end < 0) end = size();

        assert(end - begin != 0);
        assert(gpu_buffers_);

        int count = align_gpu(end - begin);
        auto& data = const_cast<TraversalData<traversal_gpu::Node>&>(c_data);
        auto nodes = reinterpret_cast<traversal_gpu::Node*>(data.nodes.data());

        anydsl::copy(ray_buffer_, begin, dev_ray_buffer_, begin, end - begin);

        traversal_gpu::intersect_gpu_instanced(
            data.root,
            nodes,
            (traversal_gpu::InstanceNode*)data.instances.data(),
            (traversal_gpu::Vec4*)data.tris.data(),
            (traversal_gpu::Ray*)dev_ray_buffer_.data() + begin,
            (traversal_gpu::Hit*)dev_hit_buffer_.data() + begin,
            count);

        anydsl::copy(dev_hit_buffer_, begin, hit_buffer_, begin, end - begin);
    }

    /// Traverses all rays currently in the queue on the CPU. For shadow rays.
    void traverse_occluded_cpu(const TraversalData<traversal_cpu::Node>& c_data, int begin = 0, int end = -1) {
        if (end < 0) end = size(); // TODO: adapt for version with offsets

        // assert(size() != 0);

        // int count = align_cpu(size());
        // auto& data = const_cast<TraversalData<traversal_cpu::Node>&>(c_data);

        // auto nodes = reinterpret_cast<traversal_cpu::Node*>(data.nodes.data());

        // traversal_cpu::occluded_cpu_instanced(
        //     data.root,
        //     nodes,
        //     data.instances.data(),
        //     data.tris.data(),
        //     ray_buffer_.data(),
        //     hit_buffer_.data(),
        //     count);
    }

    // Traverses all rays currently in the queue on the GPU. For shadow rays.
    void traverse_occluded_gpu(const TraversalData<traversal_gpu::Node>& c_data, int begin = 0, int end = -1) {
        if (end < 0) end = size();

        assert(end - begin != 0);
        assert(gpu_buffers_);

        int count = align_gpu(end - begin);
        auto& data = const_cast<TraversalData<traversal_gpu::Node>&>(c_data);
        auto nodes = reinterpret_cast<traversal_gpu::Node*>(data.nodes.data());

        anydsl::copy(ray_buffer_, begin, dev_ray_buffer_, begin, end - begin);

        traversal_gpu::occluded_gpu_instanced(
            data.root,
            nodes,
            (traversal_gpu::InstanceNode*)data.instances.data(),
            (traversal_gpu::Vec4*)data.tris.data(),
            (traversal_gpu::Ray*)dev_ray_buffer_.data() + begin,
            (traversal_gpu::Hit*)dev_hit_buffer_.data() + begin,
            count);

        anydsl::copy(dev_hit_buffer_, begin, hit_buffer_, begin, end - begin);
    }

private:
    anydsl::Array<Ray> ray_buffer_;
    anydsl::Array<Hit> hit_buffer_;

    /// True if the buffers for rays and hits on the GPU have been initialized.
    bool gpu_buffers_;
    anydsl::Array<Ray> dev_ray_buffer_;
    anydsl::Array<Hit> dev_hit_buffer_;

    std::vector<StateType> state_buffer_;
    std::atomic<int> last_;

    // Used for sorting the hit points with counting sort
    std::vector<int> sorted_indices_;
};

} // namespace imba

#endif // IMBA_RAY_QUEUE
