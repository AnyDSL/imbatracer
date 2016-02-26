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

        u = u * static_cast<float>(img_.width() - 1);
        v = (1.0f - v) * static_cast<float>(img_.height()) + v - 1.0f;

        float col, row;
        float u_fract = modff(u, &col);
        float v_fract = modff(v, &row);

        int below = (row + 1) >= img_.height() ? 0 : row + 1;
        int right = (col + 1) >= img_.width() ? 0 : col + 1;

        auto top_left  = img_(col, row);
        auto bot_left  = img_(col, below);
        auto top_right = img_(right, row);
        auto bot_right = img_(right, below);

        auto interp_top = lerp(top_left, top_right, u_fract);
        auto interp_bot = lerp(bot_left, bot_right, u_fract);
        auto interp = lerp(interp_top, interp_bot, v_fract);

        return interp;
    }

    const Image& image() const { return img_; }

private:
    Image img_;
};

using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;

} // namespace imba

#endif
