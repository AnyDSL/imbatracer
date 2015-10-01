#ifndef IMBA_RAY_QUEUE_H
#define IMBA_RAY_QUEUE_H

#include "traversal.h"
#include "../core/allocator.h"

#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <atomic>

namespace imba {

class RayQueue {
public:
    RayQueue() {}
    RayQueue(int capacity, int state_size, const void* const initial_state) 
        : ray_buffer_(capacity), hit_buffer_(capacity), last_(-1), state_size_(state_size), state_buffer_(state_size * capacity), initial_state_(initial_state)
    { 
        assert(state_size > sizeof(int) && "state size needs to be at least large enough to store a pixel index");
    }
    
    void resize(int capacity, int state_size, const void* const initial_state) {
        assert(state_size > sizeof(int) && "state size needs to be at least large enough to store a pixel index");
        
        ray_buffer_.resize(capacity);
        hit_buffer_.resize(capacity);
        last_ = -1;
        state_size_ = state_size;
        state_buffer_.resize(state_size * capacity);
        initial_state_ = initial_state;
    }

    int size() const { return last_ + 1; }
    
    ::Ray* rays() {
        return ray_buffer_.data();
    }
    
    void* states() {
        return state_buffer_.data();
    }
    
    ::Hit* hits() {
        return hit_buffer_.data();
    }
    
    void clear() {
        last_ = -1;
    }
    
    // Adds a single secondary or shadow ray to the queue. Thread-safe
    void push(const Ray& ray, const void* state) {     
        int id = ++last_; // atomic inc. of last_

        assert(id < ray_buffer_.size() && "ray queue full");
        assert(state && "no state passed to the queue for a secondary / shadow ray");        
        
        ray_buffer_[id] = ray;
        std::memcpy(state_buffer_.data() + id * state_size_, state, state_size_);
    }
    
    // Adds a set of camera rays to the queue. Thread-safe
    template<typename RayIter, typename PixelIter> 
    void push(RayIter rays_begin, RayIter rays_end, PixelIter pixels_begin, PixelIter pixels_end) {
        int count = rays_end - rays_begin;
        int end_idx = last_ += count; // atomic add to last_
        
        assert(end_idx < ray_buffer_.size() && "ray queue full");
        
        int start_idx = end_idx - (count - 1);
        
        std::copy(rays_begin, rays_end, ray_buffer_.begin() + start_idx);
        
        // copy the initial state for all rays and set the pixel index
        PixelIter p = pixels_begin;
        for (int i = 0; i < count; ++i, ++p) {
            auto location = state_buffer_.data() + (start_idx + i) * state_size_;
            std::memcpy(location, initial_state_, state_size_);
            *reinterpret_cast<int*>(location) = *p;
        }
    }
    
private:
    ThorinVector<::Ray> ray_buffer_;
    ThorinVector<::Hit> hit_buffer_;
    int state_size_;
    std::vector<unsigned char> state_buffer_;
    std::atomic<int> last_;
    const void* initial_state_;
};

}

#endif
