#ifndef IMBA_TEXTURE_LOADER_H
#define IMBA_TEXTURE_LOADER_H

#include "logger.h"
#include "path.h"

#include "../render/image.h"

namespace imba {
	
class TextureLoader {
public:
    virtual bool check_format(const Path& path) = 0;
    virtual bool load_file(const Path& path, Image& texture, Logger* logger = nullptr) = 0;
};
    
/// PNG image loader. Supports images with or wihout alpha channel.
class PngLoader : public TextureLoader {
public:
    virtual bool check_format(const Path& path) override;
    virtual bool load_file(const Path& path, Image& texture, Logger* logger = nullptr) override;
};

class TgaLoader : public TextureLoader{
public:
    virtual bool check_format(const Path& path) override;
    virtual bool load_file(const Path& path, Image& texture, Logger* logger = nullptr) override;
};
	
} // namespace imba

#endif