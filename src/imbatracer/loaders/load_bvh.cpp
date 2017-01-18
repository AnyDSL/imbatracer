#include <string>
#include <vector>
#include <fstream>
#include <anydsl_runtime.hpp>

#include "traversal.h"
#include "../core/common.h"

namespace imba {

enum class BlockType {
    BVH = 1,
    MBVH = 2
};

struct Header {
    uint32_t node_count;
    uint32_t prim_count;
};

inline bool check_header(std::istream& is) {
    uint32_t magic;
    is.read((char*)&magic, sizeof(uint32_t));
    return magic == 0x313F1A57;
}

inline bool locate_block(std::istream& is, BlockType type) {
    uint32_t block_type;
    uint64_t offset = 0;
    do {
        is.seekg(offset, std::istream::cur);

        is.read((char*)&offset, sizeof(uint64_t));
        if (is.gcount() != sizeof(uint64_t)) return false;
        is.read((char*)&block_type, sizeof(uint32_t));
        if (is.gcount() != sizeof(uint32_t)) return false;

        offset -= sizeof(BlockType);
    } while (!is.eof() && block_type != (uint32_t)type);

    return static_cast<bool>(is);
}

#ifdef GPU_TRAVERSAL
    static const BlockType block_type = BlockType::BVH;
#else
    static const BlockType block_type = BlockType::MBVH;
#endif

bool load_accel(const std::string& filename, std::vector<Node>& nodes_out, std::vector<Vec4>& tris_out, const int tri_id_offset) {
    // Account for the nodes of other BVHs that might already be inside the array.
    const int node_offset = nodes_out.size();
    const int tris_offset = tris_out.size();

    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, block_type))
        return false;

    Header h;
    in.read((char*)&h, sizeof(Header));

    nodes_out.resize(nodes_out.size() + h.node_count);
    tris_out .resize( tris_out.size() + h.prim_count);

    in.read((char*)(nodes_out.data() + node_offset), sizeof(Node) * h.node_count);
    in.read((char*)( tris_out.data() + tris_offset), sizeof(Vec4) * h.prim_count);

    if (node_offset > 0) {
        for (int i = node_offset; i < nodes_out.size(); ++i) {
#ifdef GPU_TRAVERSAL
            if (nodes_out[i].left < 0)
                nodes_out[i].left = ~(~nodes_out[i].left + tris_offset);
            else
                nodes_out[i].left += node_offset;

            if (nodes_out[i].right < 0)
                nodes_out[i].right = ~(~nodes_out[i].right + tris_offset);
            else
                nodes_out[i].right += node_offset;
#else
            for (int j = 0; j < 4; ++j) {
                if (nodes_out[i].children[j] > 0)
                    nodes_out[i].children[j] += node_offset;
                else if (nodes_out[i].children[j] < 0)
                    nodes_out[i].children[j] = ~(~nodes_out[i].children[j] + tris_offset);
            }
#endif
        }
    }

    if (tri_id_offset != 0) {
        for (int i = tris_offset; i < tris_out.size(); ) {
#ifdef GPU_TRAVERSAL
            i++;
            tris_out[i++].w = int_as_float(float_as_int(tris_out[i-1].w) + tri_id_offset);
            i++;
#else
            for (int j = 0; j < 13; ++j)
                i++;

            auto set = [tri_id_offset] (float& val) {
                if (float_as_int(val) != 0x80000000)
                    val = int_as_float(float_as_int(val) + tri_id_offset);
            };

            set(tris_out[i - 1].x);
            set(tris_out[i - 1].y);
            set(tris_out[i - 1].z);
            set(tris_out[i - 1].w);

            if (float_as_int(tris_out[i].x) == 0x80000000)
                i++; // Skip the sentinel
#endif
        }
    }

    return true;
}

bool store_accel(const std::string& filename, const std::vector<Node>& nodes, const int node_offset, const std::vector<Vec4>& tris, const int tris_offset, const int tri_id_offset) {
    // Check if the file exists and has the correct signature.
    bool exists;
    {
        std::ifstream in(filename, std::ifstream::binary);
        exists = in && check_header(in);

        if (exists && locate_block(in, block_type))
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
    Header h;
    h.node_count = nodes.size() - node_offset;
    h.prim_count = tris.size() - tris_offset;

    const uint64_t block_size = sizeof(uint32_t) + sizeof(Header) + sizeof(Node) * h.node_count + sizeof(Vec4) * h.prim_count;
    out.write((char*)&block_size, sizeof(uint64_t));
    out.write((const char*)&block_type, sizeof(uint32_t));
    out.write((char*)&h, sizeof(Header));

    // Write the actual data.
    if (node_offset == 0)
        out.write((const char*)nodes.data(), sizeof(Node) * h.node_count);
    else {
        std::vector<Node> buf;
        for (int i = node_offset; i < nodes.size(); ++i) {
            buf.emplace_back(nodes[i]);

#ifdef GPU_TRAVERSAL
            if (buf.back().left < 0)
                buf.back().left = ~(~buf.back().left - tris_offset);
            else
                buf.back().left -= node_offset;

            if (buf.back().right < 0)
                buf.back().right = ~(~buf.back().right - tris_offset);
            else
                buf.back().right -= node_offset;
#else
            for (int j = 0; j < 4; ++j) {
                if (buf.back().children[j] > 0)
                    buf.back().children[j] -= node_offset;
                else if (buf.back().children[j] < 0)
                    buf.back().children[j] = ~(~buf.back().children[j] - tris_offset);
            }
#endif
        }

        out.write((const char*)buf.data(), sizeof(Node) * h.node_count);
    }

    if (tri_id_offset != 0) {
        std::vector<Vec4> buf;
        for (int i = tris_offset; i < tris.size(); ) {

#ifdef GPU_TRAVERSAL
            buf.emplace_back(tris[i++]);
            buf.emplace_back(tris[i++]);

            buf.back().w = int_as_float(float_as_int(buf.back().w) - tri_id_offset);

            buf.emplace_back(tris[i++]);
#else
            for (int j = 0; j < 13; ++j)
                buf.emplace_back(tris[i++]);

            auto set = [tri_id_offset] (float& val) {
                if (float_as_int(val) != 0x80000000)
                    val = int_as_float(float_as_int(val) - tri_id_offset);
            };

            set(buf.back().x);
            set(buf.back().y);
            set(buf.back().z);
            set(buf.back().w);

            if (float_as_int(tris[i].x) == 0x80000000)
                buf.emplace_back(tris[i++]); // Push the sentinel
#endif
        }

        out.write((const char*)buf.data(), sizeof(Vec4) * h.prim_count);

    } else
        out.write((const char*)(tris.data() + tris_offset), sizeof(Vec4) * h.prim_count);

    return true;
}

}
