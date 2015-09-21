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
    RayQueue(int capacity, int state_size, const char* const initial_state) 
        : ray_buffer_(capacity), last_(-1), state_size_(state_size), state_buffer_(state_size * capacity), pixel_indices_(capacity), initial_state_(initial_state)
    { 
    }
    
    void resize(int capacity, int state_size, const char* const initial_state) {
        ray_buffer_.resize(capacity);
        last_ = -1;
        state_size_ = state_size;
        state_buffer_.resize(state_size * capacity);
        pixel_indices_.resize(capacity);
        initial_state_ = initial_state;
    }

    int size() const { return last_ + 1; }
    
    struct Entry {
        ::Ray* rays;
        void* state_data;
        int* pixel_indices;
        int ray_count;
        
        Entry(::Ray* r, void* s, int* px, int rc) : rays(r), state_data(s), ray_count(rc), pixel_indices(px) { }
    };
    
    Entry pop() {
        unsigned int count = size();
        last_ = -1;
        return Entry(ray_buffer_.data(), state_buffer_.data(), pixel_indices_.data(), count);
    }
    
    Entry peek() {
        unsigned int count = size();
        return Entry(ray_buffer_.data(), state_buffer_.data(), pixel_indices_.data(), count);
    }
    
    // Adds a single ray to the queue. Thread-safe
    void push(const Ray& ray, void* state, int pixel_idx) {     
        int id = ++last_; // atomic inc. of last_
        assert(id < ray_buffer_.size() && "ray queue full");
        
        ray_buffer_[id] = ray;
        
        if (state)
            std::memcpy(state_buffer_.data() + id * state_size_, state, state_size_);
        else
            std::memcpy(state_buffer_.data() + id * state_size_, initial_state_, state_size_);
            
        pixel_indices_[id] = pixel_idx;
    }
    
    // Adds a set of rays to the queue. Thread-safe
    void push_rays(const std::vector<::Ray>& rays, const std::vector<int>& pixels) {
        int count = rays.size();        
        int start_idx = last_ += count; // atomic add to last_
        
        assert(start_idx < ray_buffer_.size() && "ray queue full");
        
        start_idx -= count - 1;
        
        std::copy(rays.begin(), rays.end(), ray_buffer_.begin() + start_idx);
        std::copy(pixels.begin(), pixels.end(), pixel_indices_.begin() + start_idx);
        
        // copy the initial state for all rays
        for (int i = 0; i < count; ++i)
            std::memcpy(state_buffer_.data() + (start_idx + i) * state_size_, initial_state_, state_size_);
    }
    
private:
    ThorinVector<::Ray> ray_buffer_;
    int state_size_;
    std::vector<unsigned char> state_buffer_;
    std::vector<int> pixel_indices_;
    std::atomic<int> last_;
    const char* initial_state_;
};

}

#endif
