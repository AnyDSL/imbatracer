#ifndef IMBA_IMAGE_H
#define IMBA_IMAGE_H

#include <vector>

#include "float4.h"

namespace imba {

class Image {
public:
    Image() {}
    Image(int w, int h)
        : pixels_(w * h), width_(w), height_(h)
    {}

    const float4* pixels() const { return pixels_.data(); }
    float4* pixels() { return pixels_.data(); }

    const float4* row(int i)  const { return pixels() + i * width_; }
    float4* row(int i) { return pixels() + i * width_; }
    
    const float4& operator () (int x, int y) const { return pixels_[y * width_ + x]; }
    float4& operator () (int x, int y) { return pixels_[y * width_ + x]; }
    
    int width() const { return width_; }
    int height() const { return height_; }

    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        pixels_.resize(width_ * height_);
    }

private:
    std::vector<float4> pixels_;
    int width_, height_;
};

} // namespace imba

#endif // IMBA_IMAGE_H
