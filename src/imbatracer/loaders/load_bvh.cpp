#include <string>
#include <vector>
#include <fstream>
#include <thorin_runtime.hpp>

#include "traversal.h"
#include "bvh_format.h"
#include "../render/thorin_mem.h"

namespace imba {

bool load_accel(const std::string& filename, ThorinArray<Node>& nodes_ref, ThorinArray<Vec4>& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::BVH))
        return false;

    bvh::Header h;
    in.read((char*)&h, sizeof(bvh::Header));

    nodes_ref = ThorinArray<Node>(h.node_count);
    tris_ref  = ThorinArray<Vec4>(h.prim_count);

    in.read((char*)nodes_ref.host_data(), sizeof(Node) * h.node_count);
    in.read((char*) tris_ref.host_data(), sizeof(Vec4) * h.prim_count);

    return true;
}

bool store_accel(const std::string& filename, const ThorinArray<Node>& nodes, const ThorinArray<Vec4>& tris) {
    std::ofstream out(filename, std::ofstream::binary);
    if (!out)
        return false;

    uint32_t magic = 0x313F1A57;
    out.write((char*)&magic, sizeof(uint32_t));

    // Write the block header: size, type, header data
    const uint64_t block_size = sizeof(uint32_t) + sizeof(bvh::Header) + sizeof(Node) * nodes.size() + sizeof(Vec4) * tris.size();
    out.write((char*)&block_size, sizeof(uint64_t));
    const BlockType block_type = BlockType::BVH;
    out.write((char*)&block_type, sizeof(uint32_t));

    bvh::Header h;
    h.node_count = nodes.size();
    h.prim_count = tris.size();
    h.vert_count = 0; // unused

    out.write((char*)&h, sizeof(bvh::Header));

    // Write the actual data.
    out.write((char*)nodes.host_data(), sizeof(Node) * h.node_count);
    out.write((char*) tris.host_data(), sizeof(Vec4) * h.prim_count);

    return true;
}

}