#ifndef IMBA_IMAGE_BUFFER_HPP
#define IMBA_IMAGE_BUFFER_HPP

#include "../common/memory.hpp"
#include "../common/vector.hpp"

namespace imba {

/// Image buffer with aligned storage.
class ImageBuffer {
public:
    ImageBuffer(int width, int height, int row_stride)
        : pixels_(row_stride * height),
          width_(width), height_(height),
          stride_(row_stride)
    {}

    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }

    typedef Vec4 Pixel;

    const Pixel* pixels() const { return pixels_.data(); }
    Pixel* pixels() { return pixels_.data(); }

    const Pixel* row(int i) const { return pixels_.data() + i * stride_; }
    Pixel* row(int i) { return pixels_.data() + i * stride_; } 

private:
    std::vector<Pixel, ThorinAllocator<Pixel> > pixels_;
    int width_, height_;
    int stride_;
};

} // namespace imba

#endif // IMBA_IMAGE_BUFFER_HPP

