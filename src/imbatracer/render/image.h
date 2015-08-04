#ifndef IMAGE_H
#define IMAGE_H

namespace imba {

class Image {
public:
    Image() {}
    Image(int w, int h)
        : pixels_.resize(w * h * 4)
    {}

    const float* pixels() const { return pixels_.data(); }
    float* pixels() { return pixels_.data(); }

    int width() const { return width; }
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
