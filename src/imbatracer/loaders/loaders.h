#ifndef IMBA_LOADERS_H
#define IMBA_LOADERS_H

#include "../core/image.h"
#include "path.h"
#include "load_obj.h"
#include "load_zmod.h"

namespace imba {

bool load_png(const Path&, Image&);
bool load_tga(const Path&, Image&);

inline bool load_image(const Path& path, Image& image) {
    if (!load_png(path, image)) {
        return load_tga(path, image);
    }
    return true;
}

} // namespace imba

#endif // IMBA_LOADERS_H
