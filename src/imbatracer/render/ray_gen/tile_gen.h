#ifndef IMBA_TILE_GEN_H
#define IMBA_TILE_GEN_H

#include "imbatracer/render/ray_gen/ray_gen.h"

#include <numeric>

// Macro for empty loop bodies to suppress compiler warnings
#define NO_OP ((void)0)

namespace imba {

/// Interface for tile generators, i.e. classes that generate RayGen objects for subsets of an image.
template<typename StateType>
class TileGen {
protected:
    struct RayGenDeleter {
        void operator () (RayGen<StateType>* ptr) const {
            if (ptr) ptr->~RayGen<StateType>();
        }
    };

public:
    virtual ~TileGen() {}

    typedef std::unique_ptr<RayGen<StateType>, RayGenDeleter> TilePtr;

    /// Obtains a tile and creates a RayGen object for it. Uses the given pointer instead of allocating memory.
    virtual TilePtr next_tile(uint8_t*) = 0;
    /// Returns the size of the storage required to store a RayGen object.
    virtual size_t sizeof_ray_gen() const = 0;
    /// Restarts the frame.
    virtual void start_frame() = 0;
};

/// Generates quadratic tiles of a fixed size.
template<typename StateType>
class DefaultTileGen : public TileGen<StateType> {
    using typename TileGen<StateType>::TilePtr;

public:
    DefaultTileGen(int w, int h, int spp, int tilesize)
        : tile_size_(tilesize), spp_(spp), width_(w), height_(h)
    {
        // Compute the number of tiles required to cover the entire image.
        tiles_per_row_ = width_ / tile_size_ + (width_ % tile_size_ == 0 ? 0 : 1);
        tiles_per_col_ = height_ / tile_size_ + (height_ % tile_size_ == 0 ? 0 : 1);
        tile_count_ = tiles_per_row_ * tiles_per_col_;
    }

    TilePtr next_tile(uint8_t* mem) override final {
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

            return TilePtr(new (mem) TiledRayGen<StateType>(tile_pos_x, tile_pos_y, tile_width, tile_height, spp_, width_, height_));
        }

        return nullptr;
    }

    size_t sizeof_ray_gen() const override final {
        return sizeof(TiledRayGen<StateType>);
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

/// Generates "tiles" for ligth tracing: every tile corresponds to a set of samples drawn from one light source.
/// TODO: Improve this (in a separate class) by allowing to specify the distribution of samples to the light sources,
/// i.e. by importance sampling the lights, e.g. based on their total power, or something more fancy (visibility)
template <typename StateType>
class UniformLightTileGen : public TileGen<StateType> {
    using typename TileGen<StateType>::TilePtr;

public:
    /// Initializes the tile generator
    ///
    /// \param light_count      Number of light sources in the scene
    /// \param path_count       Total number of light paths for all lights combined
    /// \param desired_per_tile Target number of rays per tile, might generate slightly more or less (due to rounding)
    UniformLightTileGen(int light_count, int path_count, int desired_per_tile)
        : light_count_(light_count)
        , path_count_(path_count)
        , desired_per_tile_(desired_per_tile)
        , rays_per_light_(light_count, path_count / light_count)
        , cumul_tiles_per_light_(light_count)
        , tile_threshold_(desired_per_tile / 2)
    {
        assert(light_count > 0);
        assert(path_count > 0);
        assert(desired_per_tile > 0);

        // Number of paths might not be a multiple of the number of lights
        // To still generate exactly path_count paths, we assign the leftovers to the first light
        rays_per_light_[0] += path_count % light_count;

        // Compute the number of tiles for every light source
        for (int i = 0; i < light_count; ++i) {
            cumul_tiles_per_light_[i] = rays_per_light_[i] / desired_per_tile;
            if ((rays_per_light_[i] % desired_per_tile) > tile_threshold_ || cumul_tiles_per_light_[i] == 0) {
                // Only add another tile for the remainder if it is big enough or there is no tile yet
                cumul_tiles_per_light_[i]++;
            }
        }

        std::partial_sum(cumul_tiles_per_light_.begin(), cumul_tiles_per_light_.end(), cumul_tiles_per_light_.begin());
    }

    TilePtr next_tile(uint8_t* mem) override final {
        int tile_id = cur_tile_++;

        // Compute the light that this tile belongs to
        if (tile_id >= cumul_tiles_per_light_.back())
            return nullptr;

        int light;
        for (light = 0; cumul_tiles_per_light_[light] <= tile_id ; ++light)
            NO_OP;

        int ray_count = desired_per_tile_;
        if (tile_id == cumul_tiles_per_light_[light] - 1) {
            // If this is the last tile for this light, assign all remaining rays to it
            int tiles = tile_id - (light == 0 ? 0 : cumul_tiles_per_light_[light - 1]);
            ray_count = rays_per_light_[light] - tiles * desired_per_tile_;
        }

        return TilePtr(new (mem) LightRayGen<StateType>(light, ray_count));
    }

    size_t sizeof_ray_gen() const override final {
        return sizeof(LightRayGen<StateType>);
    }

    void start_frame() override final {
        cur_tile_ = 0;
    }

private:
    int light_count_;
    int path_count_;
    int desired_per_tile_;
    int tile_threshold_;

    std::vector<int> rays_per_light_;
    std::vector<int> cumul_tiles_per_light_;
    std::atomic<int> cur_tile_;
};

/// Generates tiles that represent coherent subsets of an array.
template <typename StateType>
class ArrayTileGen : public TileGen<StateType> {
    using typename TileGen<StateType>::TilePtr;

public:
    ArrayTileGen() {}

    ArrayTileGen(int tile_size, int size, int samples = 1) {
        reset(tile_size, size, samples);
    }

    void reset(int tile_size, int size, int samples = 1) {
        samples_ = samples;
        tile_sz_ = tile_size;
        sz_ = size;
        tile_count_ = size / tile_size + (size % tile_size ? 1 : 0);
        cur_tile_ = 0;
    }

    TilePtr next_tile(uint8_t* mem) override final {
        int t = cur_tile_++;
        if (t >= tile_count_)
            return nullptr;

        int offset = tile_sz_ * t;
        int len = std::min(tile_sz_ * (t + 1), sz_) - offset;
        return TilePtr(new (mem) ArrayRayGen<StateType>(offset, len, samples_));
    }

    size_t sizeof_ray_gen() const override final { return sizeof(ArrayRayGen<StateType>); }

    void start_frame() override final {
        cur_tile_ = 0;
    }

private:
    int sz_;
    int tile_sz_;
    int tile_count_;
    int samples_;
    std::atomic<int> cur_tile_;
};

} // namespace imba

#endif // IMBA_TILE_GEN_H
