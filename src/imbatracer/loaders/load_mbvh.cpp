#include <string>
#include <vector>
#include <fstream>
#include <cfloat>
#include <cstring>
#include <cassert>

#include "traversal.h"
#include "bvh_format.h"

namespace imba {

bool load_accel(const std::string& filename, std::vector<Node>& nodes_out, std::vector<Vec4>& tris_out) {
    // Account for the nodes of other BVHs that might already be inside the array.
    const int node_offset = nodes_out.size();

    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::MBVH))
        return false;

    mbvh::Header h;
    in.read((char*)&h, sizeof(mbvh::Header));

    // Read nodes
    std::vector<mbvh::Node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(mbvh::Node) * h.node_count);
    assert(in.gcount() == sizeof(mbvh::Node) * h.node_count);

    // Read vertices
    std::vector<float> vertices(4 * h.vert_count);
    in.read((char*)vertices.data(), sizeof(float) * 4 * h.vert_count);
    assert(in.gcount() == sizeof(float) * 4 * h.vert_count);

    auto leaf_node = [&] (const mbvh::Node& node, int c) {
        int node_id = ~(tris_out.size());

        for (int i = 0; i < node.prim_count[c]; i++) {
            union{
                struct {
                    Vec4 v0_x, v0_y, v0_z;
                    Vec4 e1_x, e1_y, e1_z;
                    Vec4 e2_x, e2_y, e2_z;
                    Vec4 n_x, n_y, n_z;
                    Vec4 ids;
                } tri_data;
                float raw_data[4 * 13];
            } tri;

            memcpy(tri.raw_data, vertices.data() + node.children[c] * 4 + i * 4 * 13, sizeof(float) * 4 * 13);

            tris_out.push_back(tri.tri_data.v0_x);
            tris_out.push_back(tri.tri_data.v0_y);
            tris_out.push_back(tri.tri_data.v0_z);
            tris_out.push_back(tri.tri_data.e1_x);
            tris_out.push_back(tri.tri_data.e1_y);
            tris_out.push_back(tri.tri_data.e1_z);
            tris_out.push_back(tri.tri_data.e2_x);
            tris_out.push_back(tri.tri_data.e2_y);
            tris_out.push_back(tri.tri_data.e2_z);
            tris_out.push_back(tri.tri_data.n_x);
            tris_out.push_back(tri.tri_data.n_y);
            tris_out.push_back(tri.tri_data.n_z);
            tris_out.push_back(tri.tri_data.ids);
        }

        // Insert sentinel
        Vec4 sentinel = { -0.0f, -0.0f, -0.0f, -0.0f };
        tris_out.push_back(sentinel);
        return node_id;
    };

    for (int i = 0; i < h.node_count; i++) {
        const mbvh::Node& src_node = nodes[i];
        Node dst_node;
        int k = 0;
        for (int j = 0; j < 4; j++) {
            if (src_node.prim_count[j] == 0) {
                // Empty leaf
                continue;
            }

            dst_node.min_x[k] = src_node.bb[j].lx;
            dst_node.min_y[k] = src_node.bb[j].ly;
            dst_node.min_z[k] = src_node.bb[j].lz;

            dst_node.max_x[k] = src_node.bb[j].ux;
            dst_node.max_y[k] = src_node.bb[j].uy;
            dst_node.max_z[k] = src_node.bb[j].uz;

            if (src_node.prim_count[j] < 0) {
                // Inner node
                dst_node.children[k] = src_node.children[j] + node_offset;
            } else {
                // Leaf
                dst_node.children[k] = leaf_node(src_node, j);
            }

            k++;
        }

        for (; k < 4; k++) {
            dst_node.min_x[k] = 1.0f;
            dst_node.min_y[k] = 1.0f;
            dst_node.min_z[k] = 1.0f;

            dst_node.max_x[k] = -1.0f;
            dst_node.max_y[k] = -1.0f;
            dst_node.max_z[k] = -1.0f;

            dst_node.children[k] = 0;
        }

        nodes_out.push_back(dst_node);
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

        if (exists && locate_block(in, BlockType::MBVH))
            return false; // The file already contains a BVH for this platform.
    }

    // Convert the data.
    std::vector<mbvh::Node> file_nodes;
    std::vector<float>      vertices;

    // Copies the triangles of a leaf into the buffer, returns the number of triangles.
    auto write_leaf = [&] (mbvh::Node& dest_node, int child_idx, int first_tri) {
        int i = first_tri;
        while (*reinterpret_cast<const uint32_t*>(&tris[i].x) != 0x80000000u) {
            union {
                struct {
                    Vec4 v0_x, v0_y, v0_z;
                    Vec4 e1_x, e1_y, e1_z;
                    Vec4 e2_x, e2_y, e2_z;
                    Vec4 n_x, n_y, n_z;
                    Vec4 ids;
                } tri_data;
                float raw_data[4 * 13];
            } tri;

            tri.tri_data.v0_x = tris[i++];
            tri.tri_data.v0_y = tris[i++];
            tri.tri_data.v0_z = tris[i++];
            tri.tri_data.e1_x = tris[i++];
            tri.tri_data.e1_y = tris[i++];
            tri.tri_data.e1_z = tris[i++];
            tri.tri_data.e2_x = tris[i++];
            tri.tri_data.e2_y = tris[i++];
            tri.tri_data.e2_z = tris[i++];
            tri.tri_data.n_x  = tris[i++];
            tri.tri_data.n_y  = tris[i++];
            tri.tri_data.n_z  = tris[i++];
            tri.tri_data.ids  = tris[i++];

            for (int i = 0; i < 4 * 13; ++i)
                vertices.push_back(tri.raw_data[i]);
        }

        return (i - first_tri) / 13;
    };

    for (int i = node_offset; i < nodes.size(); ++i) {
        auto&      src_node = nodes[i];
        mbvh::Node dest_node;
        for (int j = 0; j < 4; ++j) {
            dest_node.bb[j].lx = src_node.min_x[j];
            dest_node.bb[j].ly = src_node.min_y[j];
            dest_node.bb[j].lz = src_node.min_z[j];

            dest_node.bb[j].ux = src_node.max_x[j];
            dest_node.bb[j].uy = src_node.max_y[j];
            dest_node.bb[j].uz = src_node.max_z[j];

            if (src_node.children[j] == 0) {
                // Empty leaf.
                dest_node.prim_count[j] = 0;
                dest_node.children[j]   = 0;
            } else if (src_node.children[j] < 0) {
                // Leaf node.
                dest_node.children[j]   = vertices.size() / 4;
                dest_node.prim_count[j] = write_leaf(dest_node, j, ~src_node.children[j]);
            } else {
                // Inner node.
                dest_node.children[j]   = src_node.children[j] - node_offset;
                dest_node.prim_count[j] = -1;
            }
        }
        file_nodes.push_back(dest_node);
    }

    // Open the file and write the BVH block.
    auto mode = exists ? std::ofstream::binary | std::ofstream::app : std::ofstream::binary;
    std::ofstream out(filename, mode);
    if (!out)
        return false;

    // Write the file header if the file did not exist already.
    if (!exists) {
        const uint32_t magic = 0x313F1A57;
        out.write((const char*)&magic, sizeof(uint32_t));
    }

    // Write the block header: size, type, header data
    const uint64_t block_size = sizeof(uint32_t) + sizeof(mbvh::Header) + sizeof(mbvh::Node) * file_nodes.size() + sizeof(float) * vertices.size();
    out.write((char*)&block_size, sizeof(uint64_t));
    const BlockType block_type = BlockType::MBVH;
    out.write((char*)&block_type, sizeof(uint32_t));

    bvh::Header h;
    h.node_count = nodes.size();
    h.vert_count = vertices.size() / 4;

    out.write((const char*)&h, sizeof(mbvh::Header));

    // Write the actual data.
    out.write((const char*)file_nodes.data(), sizeof(mbvh::Node) * file_nodes.size());
    out.write((const char*)  vertices.data(), sizeof(float)      * vertices.size());

    return true;
}

} // namespace imba
