#ifndef IMBA_IMAGE_LOADER_HPP
#define IMBA_IMAGE_LOADER_HPP

#include "../common/path.hpp"
#include "../common/logger.hpp"
#include "../scene/image.hpp"
#include "loader.hpp"

namespace imba {

/// An image loader is an interface to load an image file from a file.
/// Different loaders are derived from this base class for several image types.
template <typename Pixel>
class ImageLoader : public Loader<ImageBuffer<Pixel> > {
public:
    virtual ~ImageLoader() {}

    virtual bool check_format(const Path& path) = 0;
    virtual bool load_file(const Path& path, ImageBuffer<Pixel>& image, Logger* logger) = 0;
};

typedef ImageLoader<::TexturePixel> TextureLoader;
typedef LoaderManager<ImageBuffer<::TexturePixel> > TextureLoaderManager;

} // namespace imba

#endif // IMBA_IMAGE_LOADER_HPP

