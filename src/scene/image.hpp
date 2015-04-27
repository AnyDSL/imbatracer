#ifndef IMBA_IMAGE_HPP
#define IMBA_IMAGE_HPP

#include <vector>
#include <cassert>
#include "../impala/impala_interface.h"
#include "../common/math.hpp"
#include "../common/memory.hpp"
#include "../common/vector.hpp"

namespace imba {

using ::GBufferPixel;
using ::TexturePixel;

/// Image buffer with aligned storage.
template <typename T>
class ImageBuffer {
public:
    ImageBuffer()
        : width_(0)
        , height_(0)
        , levels_(1)
    {}

    ImageBuffer(ImageBuffer&& other) {
        pixels_ = std::move(other.pixels_);
        width_ = other.width_;
        height_ = other.height_;
        levels_ = other.levels_;
    }

    ImageBuffer(int width, int height)
        : pixels_(width * height),
          width_(width), height_(height),
          levels_(1)
    {}

    ImageBuffer(int width, int height, int levels)
        : pixels_(size(width, height, levels)),
          width_(width), height_(height),
          levels_(levels)
    {
        assert(is_pow2(width) && is_pow2(height));
    }

    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        levels_ = 1;
        pixels_.resize(width_ * height);
    }

    void resize(int width, int height, int levels) {
        assert(is_pow2(width) && is_pow2(height));
        width_ = width;
        height_ = height;
        levels_ = levels;
        pixels_.resize(size(width, height, levels));
    }

    int width() const { return width_; }
    int height() const { return height_; }
    int levels() const { return levels_; }

    const T* pixels() const { return pixels_.data(); }
    T* pixels() { return pixels_.data(); }

    const T* row(int i) const { assert(i < height_); return pixels_.data() + i * width_; }
    T* row(int i) { assert(i < height_);  return pixels_.data() + i * width_; }

    const T* pixels(int level) const {
        assert(level < levels_);
        return pixels_.data() + offset(width_, height_, level);
    }

    T* pixels(int level) {
        assert(level < levels_);
        return pixels_.data() + offset(width_, height_, level);
    }

    const T* row(int i, int level) const {
        assert(level < levels_);
        assert(i < (height_ >> level));
        return pixels_.data() + offset(width_, height_, level) + i * (width_ >> level);
    }

    T* row(int i, int level) {
        assert(level < levels_);
        assert(i < (height_ >> level));
        return pixels_.data() + offset(width_, height_, level) + i * (width_ >> level);
    }

    typedef T Pixel;

protected:
    static int offset(int width, int height, int level) {
        const int s = width * height;
        return 4 * (s - (s >> (2 * level))) / 3;
    }

    static int size(int width, int height, int levels) {
        return offset(width, height, levels);
    }

    ThorinVector<T> pixels_;
    int width_, height_;
    int levels_;
};

typedef ImageBuffer<GBufferPixel> GBuffer;
typedef ImageBuffer<TexturePixel> Texture;

/// Generate mipmaps for a texture, using bilinear filtering.
inline void generate_mipmaps(Texture& tex) {
    assert(is_pow2(tex.width()) && is_pow2(tex.height()));

    // Find number of levels
    int levels = 1, p = 2;
    while (tex.width()  / p >= 2 &&
           tex.height() / p >= 2) {
        levels++;
        p *= 2;
    }

    // Generate each level
    tex.resize(tex.width(), tex.height(), levels);
    for (int i = 1, p = 2; i < levels; i++, p *= 2) {
        const int width = tex.width() / p;
        const int height = tex.height() / p;

        // Bilinear filtering on higher scale to get lower scale
        for (int y = 0; y < height; y++) {
            const TexturePixel* r0 = tex.row(y * 2 + 0, i - 1);
            const TexturePixel* r1 = tex.row(y * 2 + 1, i - 1);
            TexturePixel* row = tex.row(y, i);

            for (int x = 0; x < width; x++) {
                const TexturePixel* p0 = r0 + x * 2;
                const TexturePixel* p1 = r1 + x * 2;

                row[x].r = (p0[0].r + p0[1].r + p1[0].r + p1[1].r) * 0.25f;
                row[x].g = (p0[0].g + p0[1].g + p1[0].g + p1[1].g) * 0.25f;
                row[x].b = (p0[0].b + p0[1].b + p1[0].b + p1[1].b) * 0.25f;
                row[x].a = (p0[0].a + p0[1].a + p1[0].a + p1[1].a) * 0.25f;
            }
        }
    }
}

/// Transforms the texture into a grayscale image.
inline Texture grayscale(const Texture& tex) {
    Texture res(tex.width(), tex.height());

    for (int i = 0; i < tex.width() * tex.height(); i++) {
        const TexturePixel* in = tex.pixels() + i;
        TexturePixel* out = res.pixels() + i;
        out->r = out->g = out->b = out->a = (in->r + in->g + in->b) * (1.0f / 3.0f);
    }

    return res;
}

} // namespace imba

#endif // IMBA_IMAGE_HPP

