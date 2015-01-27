#ifndef IMBA_IMAGE_HPP
#define IMBA_IMAGE_HPP

#include <vector>
#include "../impala/impala_interface.h"
#include "../common/memory.hpp"
#include "../common/vector.hpp"

namespace imba {

/// Image buffer with aligned storage.
template <typename T>
class ImageBuffer {
public:
    ImageBuffer()
        : width_(0)
        , height_(0)
        , stride_(0)
    {}

    ImageBuffer(int width, int height)
        : pixels_(width * height * sizeof(T)),
          width_(width), height_(height),
          stride_(width * sizeof(T))
    {}

    ImageBuffer(int width, int height, int row_stride)
        : pixels_(row_stride * height),
          width_(width), height_(height),
          stride_(row_stride)
    {}

    void resize(int width, int height, int row_stride) {
        width_ = width;
        height_ = height;
        stride_ = row_stride;
        pixels_.resize(row_stride * height);
    }

    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        stride_ = sizeof(T) * width;
        pixels_.resize(stride_ * height);
    }

    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }

    const T* pixels() const { return reinterpret_cast<const T*>(pixels_.data()); }
    T* pixels() { return reinterpret_cast<T*>(pixels_.data()); }

    const T* row(int i) const { return reinterpret_cast<const T*>(pixels_.data() + i * stride_); }
    T* row(int i) { return reinterpret_cast<T*>(pixels_.data() + i * stride_); }

    typedef T Pixel;

protected:
    ThorinVector<char> pixels_;
    int width_, height_;
    int stride_;
};

typedef ImageBuffer<::GBufferPixel> GBuffer;
typedef ImageBuffer<::TexturePixel> Texture;

} // namespace imba

#endif // IMBA_IMAGE_HPP

