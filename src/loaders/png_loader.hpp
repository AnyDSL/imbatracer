#ifndef IMBA_PNG_LOADER_HPP
#define IMBA_PNG_LOADER_HPP

#include "image_loader.hpp"

namespace imba {

/// PNG image loader. Supports images with or wihout alpha channel.
class PngLoader : public TextureLoader {
    virtual bool check_format(const Path& path) override;
    virtual bool load_file(const Path& path, Texture& texture, Logger* logger = nullptr) override;
};

} // namespace imba

#endif

