#ifndef IMAGE_H
#define IMAGE_H

#include "thorin_mem.h"
#include "../core/float4.h"

#include <vector>

namespace imba {

class Image {
public:
    Image() {}
    Image(int w, int h)
        : pixels_(w * h * 4), width_(w), height_(h)
    {}

    const float* pixels() const { return pixels_.data(); }
    float* pixels() { return pixels_.data(); }
    float* row(int i) { return pixels() + i * width_ * 4; }
    
    const float4 get(int i) const { return float4(pixels_[i * 4], pixels_[i * 4 + 1], pixels_[i * 4 + 2], pixels_[i * 4 + 3]); }
    const float4 get(int col, int row) const { return get(row * width_ + col); }
    
    void set(int i, const float4& value) {
        pixels_[i * 4]     = value.x;
        pixels_[i * 4 + 1] = value.y;
        pixels_[i * 4 + 2] = value.z;
        pixels_[i * 4 + 3] = value.w;
    }

    int width() const { return width_; }
    int height() const { return height_; }

    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        pixels_.resize(width_ * height_ * 4);
    }

private:
    std::vector<float> pixels_;
    int width_, height_;
};

} // namespace imba

#endif // IMAGE_H
