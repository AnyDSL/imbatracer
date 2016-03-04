#ifndef IMBA_RAY_GEN_H
#define IMBA_RAY_GEN_H

#include "ray_queue.h"
#include "random.h"
#include <cfloat>
#include <random>
#include <functional>

namespace imba {

template <typename StateType>
class RayGen {
public:
    typedef std::function<void (int, int, ::Ray&, StateType&)> SamplePixelFn;
    virtual void fill_queue(RayQueue<StateType>&, SamplePixelFn) = 0;
    virtual void start_frame() = 0;
    virtual int rays_left() const = 0;
};

/// Base class for all classes that generate rays per pixel (camera, lights)
template <typename StateType>
class PixelRayGen : public RayGen<StateType> {
public:
    PixelRayGen(int w, int h, int spp)
        : width_(w), height_(h), n_samples_(spp)
    {}

    int width() { return width_; }
    int height() { return height_; }
    int num_samples() { return n_samples_; }

    virtual void start_frame() { next_pixel_ = 0; }

    virtual int rays_left() const { return (n_samples_ * width_ * height_) - next_pixel_; }

    virtual void fill_queue(RayQueue<StateType>& out, typename RayGen<StateType>::SamplePixelFn sample_pixel) override {
        // only generate at most n samples per pixel
        if (next_pixel_ >= n_samples_ * width_ * height_) return;

        // calculate how many rays are needed to fill the queue
        int count = out.capacity() - out.size();
        if (count <= 0) return;

        // make sure that no pixel is sampled more than n_samples_ times
        if (next_pixel_ + count > n_samples_ * width_ * height_) {
            count = n_samples_ * width_ * height_ - next_pixel_;
        }

        static std::random_device rd;
        uint64_t seed_base = rd();
        for (int i = next_pixel_; i < next_pixel_ + count; ++i) {
            // Compute coordinates, id etc.
            int pixel_idx = i % (width_ * height_);
            int sample_idx = i / (width_ * height_);
            int y = pixel_idx / width_;
            int x = pixel_idx % width_;

            // Create the ray and its state.
            StateType state;
            ::Ray ray;

            state.pixel_id = pixel_idx;
            state.sample_id = sample_idx;

            // Use Bernstein's hash function to scramble the seed base value
            int seed = seed_base;
            seed = 33 * seed ^ i;
            seed = 33 * seed ^ i;
            seed = 33 * seed ^ i;
            seed = 33 * seed ^ i;
            state.rng = RNG(seed);
            state.rng.discard((seed % 5) + 16 + pixel_idx % 5);
            sample_pixel(x, y, ray, state);

            out.push(ray, state);
        }

        // store which pixel has to be sampled next
        next_pixel_ += count;
    }

protected:
    int next_pixel_;
    int width_;
    int height_;
    int n_samples_;
};

} // namespace imba

#endif
