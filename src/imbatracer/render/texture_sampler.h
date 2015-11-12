#ifndef IMBA_TEXTURE_SAMPLER
#define IMBA_TEXTURE_SAMPLER

#include "image.h"

#include <memory>
#include <vector>

namespace imba {
	
class TextureSampler {
public:
	TextureSampler(std::unique_ptr<Image> img) : img_(std::move(img)) {}
	
	TextureSampler(const TextureSampler&) = delete;
	TextureSampler& operator=(const TextureSampler&) = delete;
	
	float4 sample(float u, float v) {
		int col = clamp(static_cast<int>(u * img_->width()), 0, img_->width() - 1);
		int row = clamp(static_cast<int>(v * img_->height()), 0, img_->height() - 1);
		
		return img_->get(col, row);
	}	
	
private:
	std::unique_ptr<Image> img_;
};	
	
using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;
	
} // namespace imba

#endif