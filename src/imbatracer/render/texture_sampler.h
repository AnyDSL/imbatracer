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
	
	float4 sample(float u, float v) {
		float intpart;
		u = modff(u, &intpart);
		v = modff(v, &intpart);
		if (u < 0.0f) u += 1.0f;
		if (v < 0.0f) v += 1.0f;
		
		int col = u * (img_.width() - 1);
		int row = v * (img_.height() - 1);
		row = (img_.height() - 1) - row;
		
		return img_(col, row);
	}	
	
private:
	Image img_;
};	
	
using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;
	
} // namespace imba

#endif
