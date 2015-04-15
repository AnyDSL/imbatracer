#ifndef IMBA_TGA_LOADER_HPP
#define IMBA_TGA_LOADER_HPP

#include "image_loader.hpp"

namespace imba {

/// TGA image loader.
class TgaLoader : public TextureLoader {
public:
    virtual bool check_format(const Path& path) override;
    virtual bool load_file(const Path& path, Texture& texture, Logger* logger = nullptr) override;
};

} // namespace imba

#endif // IMBA_TGA_LOADER_HPP

