#ifndef IMBA_RAY_QUEUE_H
#define IMBA_RAY_QUEUE_H

#include "traversal.h"
#include "../core/allocator.h"

#include <vector>
#include <cstring>
#include <assert.h>

namespace imba {

class RayQueue {
public:
    RayQueue() {}
    RayQueue(int capacity, int state_size, void* initial_state) 
        : ray_buffer_(capacity), last_(-1), state_size_(state_size), state_buffer_(state_size * capacity), pixel_indices_(capacity), initial_state_(initial_state)
    { 
    }
    
    void resize(int capacity, int state_size, void* initial_state) {
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
    
    void push(const Ray& ray, void* state, int pixel_idx) {
        assert(last_ + 1 < ray_buffer_.size() && "ray queue full");
     
        ray_buffer_[++last_] = ray;
        
        if (state)
            std::memcpy(state_buffer_.data() + last_ * state_size_, state, state_size_);
        else
            std::memcpy(state_buffer_.data() + last_ * state_size_, initial_state_, state_size_);
            
        pixel_indices_[last_] = pixel_idx;
    }
    
private:
    ThorinVector<::Ray> ray_buffer_;
    int state_size_;
    std::vector<unsigned char> state_buffer_;
    std::vector<int> pixel_indices_;
    int last_;
    void* initial_state_;
};

}

#endif
