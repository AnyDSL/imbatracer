#ifndef IMBA_TEXTURE_SAMPLER
#define IMBA_TEXTURE_SAMPLER

#include "../core/image.h"

#include <memory>
#include <vector>

namespace imba {

class TextureSampler {
public:
    TextureSampler(Image&& img) : img_(std::move(img)) {}

    TextureSampler(const TextureSampler&) = delete;
    TextureSampler& operator=(const TextureSampler&) = delete;

    float4 sample(float2 uv) {
        float u = clamp(uv.x - (int)uv.x, -1.0f, 1.0f);
        float v = clamp(uv.y - (int)uv.y, -1.0f, 1.0f);
        u += u < 0.0f ? 1.0f : 0.0f;
        v += v < 0.0f ? 1.0f : 0.0f;
        v = 1.0f - v;

        const float kx = u * (img_.width()  - 1);
        const float ky = v * (img_.height() - 1);

        const int x0 = (int)kx;
        const int y0 = (int)ky;
        const int x1 = (x0 + 1) % img_.width();
        const int y1 = (y0 + 1) % img_.height();

        const float gx = kx - floorf(kx);
        const float gy = ky - floorf(ky);
        const float hx = 1.0f - gx;
        const float hy = 1.0f - gy;

        const float4 i00 = img_(x0, y0);
        const float4 i10 = img_(x1, y0);
        const float4 i01 = img_(x0, y1);
        const float4 i11 = img_(x1, y1);

        return hy * (hx * i00 + gx * i10) +
               gy * (hx * i01 + gx * i11);
    }

    const Image& image() const { return img_; }

private:
    Image img_;
};

using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;

} // namespace imba

#endif
