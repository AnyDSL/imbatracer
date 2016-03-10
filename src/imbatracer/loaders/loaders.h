#ifndef IMBA_LOADERS_H
#define IMBA_LOADERS_H

#include "../core/image.h"
#include "path.h"
#include "load_obj.h"

#include "../render/thorin_mem.h"
#include "traversal.h"

namespace imba {

bool load_png(const Path&, Image&);
bool load_tga(const Path&, Image&);

inline bool load_image(const Path& path, Image& image) {
    if (!load_png(path, image)) {
        return load_tga(path, image);
    }
    return true;
}

bool load_accel(const std::string& filename, ThorinArray<Node>& nodes_ref, ThorinArray<Vec4>& tris_ref);
bool store_accel(const std::string& filename, const ThorinArray<Node>& nodes, const ThorinArray<Vec4>& tris);

} // namespace imba

#endif // IMBA_LOADERS_H
