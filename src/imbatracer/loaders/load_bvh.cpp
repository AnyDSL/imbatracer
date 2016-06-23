#include <string>
#include <vector>
#include <fstream>
#include <thorin_runtime.hpp>

#include "traversal.h"
#include "bvh_format.h"

namespace imba {

bool load_accel(const std::string& filename, std::vector<Node>& nodes_out, std::vector<Vec4>& tris_out) {
    // Account for the nodes of other BVHs that might already be inside the array.
    const int node_offset = nodes_out.size();

    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::BVH))
        return false;

    bvh::Header h;
    in.read((char*)&h, sizeof(bvh::Header));

    nodes_out.resize(nodes_out.size() + h.node_count);
    tris_out .resize( tris_out.size() + h.prim_count);

    in.read((char*)nodes_out.data(), sizeof(Node) * h.node_count);
    in.read((char*) tris_out.data(), sizeof(Vec4) * h.prim_count);

    if (node_offset > 0) {
        for (int i = node_offset; i < nodes_out.size(); ++i) {
            nodes_out[i].left  += node_offset;
            nodes_out[i].right += node_offset;
        }
    }

    return true;
}

bool store_accel(const std::string& filename, const std::vector<Node>& nodes, const int node_offset, const std::vector<Vec4>& tris) {
    // Check if the file exists and has the correct signature.
    bool exists;
    {
        std::ifstream in(filename, std::ifstream::binary);
        if (!in || !check_header(in))
            exists = false;
        else
            exists = true;

        if (exists && locate_block(in, BlockType::BVH))
            return false; // The file already contains a BVH for this platform.
    }

    // Open the file and write the BVH block.
    auto mode = exists ? std::ofstream::binary | std::ofstream::app : std::ofstream::binary;
    std::ofstream out(filename, mode);
    if (!out)
        return false;

    // Write the header if the file did not exist already.
    if (!exists) {
        const uint32_t magic = 0x313F1A57;
        out.write((const char*)&magic, sizeof(uint32_t));
    }

    // Write the block header: size, type, header data
    const uint64_t block_size = sizeof(uint32_t) + sizeof(bvh::Header) + sizeof(Node) * (nodes.size() - node_offset) + sizeof(Vec4) * tris.size();
    out.write((char*)&block_size, sizeof(uint64_t));
    const BlockType block_type = BlockType::BVH;
    out.write((char*)&block_type, sizeof(uint32_t));

    bvh::Header h;
    h.node_count = nodes.size() - node_offset;
    h.prim_count = tris.size();
    h.vert_count = 0; // unused

    out.write((char*)&h, sizeof(bvh::Header));

    // Write the actual data.
    if (node_offset == 0)
        out.write((const char*)nodes.data(), sizeof(Node) * h.node_count);
    else {
        std::vector<Node> buf(h.node_count);
        for (int i = node_offset; i < nodes.size(); ++i) {
            buf.emplace_back(nodes[i]);
            buf.back().left  -= node_offset;
            buf.back().right -= node_offset;
        }

        out.write((const char*)buf.data(), sizeof(Node) * h.node_count);
    }

    out.write((char*) tris.data(), sizeof(Vec4) * h.prim_count);

    return true;
}

}