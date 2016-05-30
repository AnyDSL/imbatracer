#ifndef IMBA_IMAGE_H
#define IMBA_IMAGE_H

#include <vector>
#include <cstring>
#include <atomic>

#include "float4.h"

namespace imba {

/// Implementation of an atomic type for float4 that supports assignment and addition.
class AtomicFloat4 {
public:
    std::atomic<float> x, y, z, w;

    AtomicFloat4(const float4& a)
        : x(a.x), y(a.y), z(a.z), w(a.w)
    {}

    AtomicFloat4() {}

    operator float4() {
        return float4(x, y, z, w);
    }

    AtomicFloat4& operator = (const float4& a) {
        x.store(a.x);
        y.store(a.y);
        z.store(a.z);
        w.store(a.w);
        return *this;
    }

    float4 operator += (const float4& a) {
        return float4(
            atomic_add(x, a.x),
            atomic_add(y, a.y),
            atomic_add(z, a.z),
            atomic_add(w, a.w));
    }

private:
    float atomic_add(std::atomic<float>& a, float b) {
        float old_val = a.load();
        float desired_val = old_val + b;
        while(!a.compare_exchange_weak(old_val, desired_val))
            desired_val = old_val + b;
        return desired_val;
    }
};

template<typename T>
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
        pixels_.resize(width_ * height_);
    }

    void clear() {
        for(auto& p : pixels_) p = float4(0.0f);
    }

    int size() { return width_ * height_; }

private:
    std::vector<T> pixels_;
    int width_, height_;
};

using Image = ImageBase<float4>;
using AtomicImage = ImageBase<AtomicFloat4>;

} // namespace imba

#endif // IMBA_IMAGE_H
