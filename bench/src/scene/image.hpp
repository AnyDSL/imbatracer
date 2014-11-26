#ifndef IMBA_IMAGE_HPP
#define IMBA_IMAGE_HPP

#include <vector>
#include "../common/memory.hpp"
#include "../common/vector.hpp"

namespace imba {

/// Image buffer with aligned storage.
class Image {
public:
    Image()
        : width_(0)
        , height_(0)
        , stride_(0)
    {}

    Image(int width, int height, int row_stride)
        : pixels_(row_stride * height),
          width_(width), height_(height),
          stride_(row_stride)
    {}

    void resize(int width, int height, int row_stride) {
        pixels_.resize(row_stride * height);
    }

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

#endif // IMBA_IMAGE_HPP

