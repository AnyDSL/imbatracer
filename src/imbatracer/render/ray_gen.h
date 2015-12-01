#ifndef IMBA_RAY_GEN_H
#define IMBA_RAY_GEN_H

#include "ray_queue.h"
#include "random.h"
#include <cfloat>

namespace imba {

struct RayGen {  
};

/// Base class for all classes that generate rays per pixel (camera, lights)
template<typename StateType>
class PixelRayGen : public RayGen{
public:
    PixelRayGen(int w, int h, int n) : width_(w), height_(h), n_samples_(n) { }

    int width() { return width_; }
    int height() { return height_; }
    int num_samples() { return n_samples_; }

    void set_target_count(int count) {
        target_count_ = count;
    }
    
    void start_frame() {
        next_pixel_ = 0;
        generated_pixels_ = 0;
    }
    
    void fill_queue(RayQueue<StateType>& out) {
        // only generate at most n samples per pixel
        if (generated_pixels_ > n_samples_ * width_ * height_) return;
        
        // calculate how many rays are needed to fill the queue
        int count = target_count_ - out.size();
        
        if (count <= 0) return;
        
        // make sure that no pixel is sampled more than n_samples_ times
        if (generated_pixels_ + count > n_samples_ * width_ * height_) {
            count = n_samples_ * width_ * height_ - generated_pixels_;
        }
        
        if (count <= 0) return;
        
        // remember how many pixel samples were generated
        generated_pixels_ += count;
        
        std::random_device rd;
        std::mt19937 seed_gen(rd());
        //uint64_t rnd_seed_base = rd();
        for (int i = next_pixel_; i < next_pixel_ + count; ++i) {
            // Compute coordinates, id etc.
            int pixel_idx = i % (width_ * height_);
            int sample_idx = i / (width_ * height_);
            int y = pixel_idx / width_;
            int x = pixel_idx % width_;
            
            // Create the ray and its state.
            StateType state;
            ::Ray ray;

            std::uniform_int_distribution<int> uniform(0, 0xFFFFFF);
            int seed = uniform(seed_gen);
            
            state.pixel_id = pixel_idx;
            state.sample_id = sample_idx;
            state.rng = RNG(seed);
            //state.rng = RNG(rnd_seed_base << ((x + y) % 16)); // TODO improve seed
            sample_pixel(x, y, ray, state);
            
            out.push(ray, state);
        }
        
        // store which pixel has to be sampled next
        next_pixel_ += count;
    }
    
protected:    
    int next_pixel_;
    int generated_pixels_;
    int target_count_;
    
    int width_;
    int height_;
    int n_samples_;
    
    virtual void sample_pixel(int x, int y, ::Ray& ray_out, StateType& state_out) = 0;
};

} // namespace imba

#endif
