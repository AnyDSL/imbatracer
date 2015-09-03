#ifndef IMAGE_H
#define IMAGE_H

#include "../core/allocator.h"
#include "../core/float4.h"

namespace imba {

class Image {
public:
    Image() {}
    Image(int w, int h)
        : pixels_(w * h * 4), width_(w), height_(h)
    {}

    const float* pixels() const { return pixels_.data(); }
    float* pixels() { return pixels_.data(); }
    
    const float4 get(int i) const { return float4(pixels_[i * 4], pixels_[i * 4 + 1], pixels_[i * 4 + 2], pixels_[i * 4 + 3]); }
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
    ThorinVector<float> pixels_;
    int width_, height_;
};

} // namespace imba

#endif // IMAGE_H
