#ifndef IMBA_RAY_GEN_H
#define IMBA_RAY_GEN_H

#include "ray_queue.h"
#include "random.h"
#include <cfloat>

namespace imba {

// base class for all classes that generate rays per pixel (camera, lights)
template <typename StateType>
class PixelRayGen {
public:
    PixelRayGen(int w, int h, int n, RayKind kind) : width_(w), height_(h), n_samples_(n), kind_(kind) { }

    int width() { return width_; }
    int height() { return height_; }
    int num_samples() { return n_samples_; }

    void set_target_count(int count) {
        pixels_.resize(count);
        rays_.resize(count);
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
        
        // remember how many pixel samples were generated
        generated_pixels_ += count;
        
        const int last_pixel = (next_pixel_ + count) % (width_ * height_);
        
        static RNG rng;      
#pragma omp parallel for private(rng)
        for (int i = next_pixel_; i < next_pixel_ + count; ++i) {
            int pixel_idx = i % (width_ * height_);
            int sample_idx = i / (width_ * height_);
            
            RayState& state = pixels_[i - next_pixel_];
            state.pixel_id = pixel_idx;
            state.sample_id = sample_idx;
            state.kind = kind_;
            
            int y = pixel_idx / width_;
            int x = pixel_idx % width_;
            
            sample_pixel(x, y, rng, rays_[i - next_pixel_]);
        }
        
        // store which pixel has to be sampled next
        next_pixel_ = last_pixel;
        out.push(rays_.begin(), rays_.begin() + count, pixels_.begin(), pixels_.begin() + count);
    }
    
protected:
    std::vector<::Ray> rays_;
    std::vector<StateType> pixels_;
    
    int next_pixel_;
    int generated_pixels_;
    int target_count_;
    
    int width_;
    int height_;
    int n_samples_;
    RayKind kind_;
    
    virtual void sample_pixel(int x, int y, RNG& rng, ::Ray& ray_out) = 0;
};

} // namespace imba

#endif
