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
    virtual ~RayGen() {}

    typedef std::function<void (int, int, ::Ray&, StateType&)> SamplePixelFn;
    virtual void fill_queue(RayQueue<StateType>&, SamplePixelFn) = 0;
    virtual void start_frame() = 0;
    virtual bool is_empty() const = 0;
};

/// Interface for tile generators, i.e. classes that generate RayGen objects for subsets of an image.
template<typename StateType>
class TileGen {
public:
    /// Obtains a tile and creates a RayGen object for it.
    virtual std::unique_ptr<RayGen<StateType> > next_tile() = 0;

    virtual void start_frame() = 0;
};

/// Generates n primary rays per pixel in range [0,0] to [w,h]
template <typename StateType>
class PixelRayGen : public RayGen<StateType> {
public:
    PixelRayGen(int w, int h, int spp)
        : width_(w), height_(h), n_samples_(spp), next_pixel_(0)
    {}

    int max_rays() const { return width_ * height_ * n_samples_; }

    virtual void start_frame() override { next_pixel_ = 0; }

    virtual bool is_empty() const override { return next_pixel_ >= max_rays(); }

    virtual void fill_queue(RayQueue<StateType>& out, typename RayGen<StateType>::SamplePixelFn sample_pixel) override {
        // only generate at most n samples per pixel
        if (next_pixel_ >= max_rays()) return;

        // calculate how many rays are needed to fill the queue
        int count = out.capacity() - out.size();
        if (count <= 0) return;

        // make sure that no pixel is sampled more than n_samples_ times
        if (next_pixel_ + count > max_rays()) {
            count = max_rays() - next_pixel_;
        }

        static std::random_device rd;
        uint64_t seed_base = rd();
        for (int i = next_pixel_; i < next_pixel_ + count; ++i) {
            // Compute coordinates, id etc.
            int pixel_idx = i / n_samples_;
            int sample_idx = i % n_samples_;
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
    const int width_;
    const int height_;
    const int n_samples_;
};

/// Generates primary rays for the pixels within a tile. Simply adds an offset to the pixel coordinates from the
/// PixelRayGen, according to the position of the tile.
template<typename StateType>
class TiledRayGen : public PixelRayGen<StateType> {
public:
    TiledRayGen(int left, int top, int w, int h, int spp, int full_width, int full_height)
        : PixelRayGen<StateType>(w, h, spp), top_(top), left_(left)
        , full_height_(full_height), full_width_(full_width)
    {}

    virtual void fill_queue(RayQueue<StateType>& out, typename RayGen<StateType>::SamplePixelFn sample_pixel) override {
        PixelRayGen<StateType>::fill_queue(out,
            [sample_pixel, this](int x, int y, ::Ray& r, StateType& s) {
                s.pixel_id = (y + top_) * full_width_ + (x + left_);
                sample_pixel(x + left_, y + top_, r, s);
            });
    }

private:
    int top_, left_;
    int full_width_, full_height_;
};

/// Generates quadratic tiles of a fixed size.
template<typename StateType>
class DefaultTileGen : public TileGen<StateType> {
public:
    DefaultTileGen(int w, int h, int spp, int tilesize)
    : tile_size_(tilesize), spp_(spp), width_(w), height_(h)
    {
        // Compute the number of tiles required to cover the entire image.
        tiles_per_row_ = width_ / tile_size_ + (width_ % tile_size_ == 0 ? 0 : 1);
        tiles_per_col_ = height_ / tile_size_ + (height_ % tile_size_ == 0 ? 0 : 1);
        tile_count_ = tiles_per_row_ * tiles_per_col_;
    }

    std::unique_ptr<RayGen<StateType> > next_tile() override final {
        int tile_id;
        while ((tile_id = cur_tile_++) < tile_count_) {
            // Get the next tile and compute its extents
            int tile_pos_x  = (tile_id % tiles_per_row_) * tile_size_;
            int tile_pos_y  = (tile_id / tiles_per_row_) * tile_size_;
            int tile_width  = std::min(width_ - tile_pos_x, tile_size_);
            int tile_height = std::min(height_ - tile_pos_y, tile_size_);

            // If the next tile is smaller than half the size, acquire it as well.
            // If this tile is smaller than half the size, skip it (was acquired by one of its neighbours)
            if (tile_width < tile_size_ / 2 ||
                tile_height < tile_size_ / 2)
                continue;

            if (width_ - (tile_pos_x + tile_width) < tile_size_ / 2)
                tile_width += width_ - (tile_pos_x + tile_width);

            if (height_ - (tile_pos_y + tile_height) < tile_size_ / 2)
                tile_height += height_ - (tile_pos_y + tile_height);

            return std::unique_ptr<RayGen<StateType> >(new TiledRayGen<StateType>(tile_pos_x, tile_pos_y, tile_width, tile_height,
                                                                                  spp_, width_, height_));
        }

        // No tiles left.
        return nullptr;
    }

    void start_frame() override final {
        cur_tile_ = 0;
    }

private:
    int tile_size_;
    int spp_, width_, height_;

    int tiles_per_row_;
    int tiles_per_col_;
    int tile_count_;

    std::atomic<int> cur_tile_;
};

} // namespace imba

#endif // IMBA_RAY_GEN_H
