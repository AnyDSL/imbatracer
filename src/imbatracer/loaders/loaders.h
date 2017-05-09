#ifndef IMBA_LOADERS_H
#define IMBA_LOADERS_H

#include "imbatracer/core/image.h"
#include "imbatracer/loaders/path.h"
#include "imbatracer/loaders/load_obj.h"
#include "imbatracer/loaders/store_png.h"

#include "imbatracer/render/scheduling/ray_queue.h"

namespace imba {

bool load_png(const Path&, Image&);
bool load_tga(const Path&, Image&);
bool load_hdr(const Path&, Image&);

inline bool load_image(const Path& path, Image& image) {
    if (!load_png(path, image)) {
        return load_tga(path, image);
    }
    return true;
}

bool load_accel_cpu (const std::string& filename, std::vector<traversal_cpu::Node>& nodes_out, std::vector<Vec4>& tris_out, const int tri_id_offset);
bool store_accel_cpu(const std::string& filename, const std::vector<traversal_cpu::Node>& nodes, const int node_offset, const std::vector<Vec4>& tris, const int tris_offset, const int tri_id_offset);

bool load_accel_gpu (const std::string& filename, std::vector<traversal_gpu::Node>& nodes_out, std::vector<Vec4>& tris_out, const int tri_id_offset);
bool store_accel_gpu(const std::string& filename, const std::vector<traversal_gpu::Node>& nodes, const int node_offset, const std::vector<Vec4>& tris, const int tris_offset, const int tri_id_offset);

} // namespace imba

#endif // IMBA_LOADERS_H
