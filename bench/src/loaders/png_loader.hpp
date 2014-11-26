#ifndef IMBA_PNG_LOADER_HPP
#define IMBA_PNG_LOADER_HPP

#include "image_loader.hpp"

namespace imba {

class PngLoader : public ImageLoader {
    virtual bool check_format(const Path& path) override;
    virtual bool load_file(const Path& path, Image& scene, Logger* logger = nullptr) override;
};

} // namespace imba

#endif

