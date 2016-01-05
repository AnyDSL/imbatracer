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
        if (u < 0.0f) u += 1.0f;
        if (v < 0.0f) v += 1.0f;

        int col = u * (img_.width() - 1);
        int row = v * (img_.height() - 1);
        row = (img_.height() - 1) - row;

        return img_(col, row);
    }

    const Image& image() const { return img_; }

private:
    Image img_;
};

using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;

} // namespace imba

#endif
