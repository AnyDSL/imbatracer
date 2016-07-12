#ifndef IMBA_IMAGE_H
#define IMBA_IMAGE_H

#include <vector>
#include <cstring>
#include <atomic>

#include "rgb.h"

namespace imba {

template <typename T>
class ImageBase {
public:
    ImageBase() {}
    ImageBase(int w, int h)
        : pixels_(w * h), width_(w), height_(h)
    {}

    const T* pixels() const { return pixels_.data(); }
    T* pixels() { return pixels_.data(); }

    const T* row(int i)  const { return pixels() + i * width_; }
    T* row(int i) { return pixels() + i * width_; }

    const T& operator () (int x, int y) const { return pixels_[y * width_ + x]; }
    T& operator () (int x, int y) { return pixels_[y * width_ + x]; }

    int width() const { return width_; }
    int height() const { return height_; }

    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        std::vector<T>(width_ * height_).swap(pixels_);
    }

    void clear() {
        for (auto& p : pixels_) p = T::zero();
    }

    int size() { return width_ * height_; }

private:
    std::vector<T> pixels_;
    int width_, height_;
};

using Image = ImageBase<rgba>;
using AtomicImage = ImageBase<atomic_rgb>;

} // namespace imba

#endif // IMBA_IMAGE_H
