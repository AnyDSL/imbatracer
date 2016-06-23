#ifndef IMBA_LOADERS_H
#define IMBA_LOADERS_H

#include "../core/image.h"
#include "path.h"
#include "load_obj.h"

//#include "../render/thorin_array.h"
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

bool load_accel(const std::string& filename, std::vector<Node>& nodes_out, std::vector<Vec4>& tris_out);
bool store_accel(const std::string& filename, const std::vector<Node>& nodes, const int node_offset, const std::vector<Vec4>& tris);

} // namespace imba

#endif // IMBA_LOADERS_H
