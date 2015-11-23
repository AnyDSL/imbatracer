#include <iostream>
#include <fstream>
#include <cstdint>
#include <memory>
#include <zlib.h>

#include "../loaders/loaders.h"

using namespace imba;

struct TriIdx {
    int v0, v1, v2;
    TriIdx(int v0, int v1, int v2) : v1(v1), v2(v2), v0(v0) { }
};

struct HashIndex {
    size_t operator () (const obj::Index& i) const {
        return i.v ^ (i.t << 7) ^ (i.n << 11);
    }
};

struct CompareIndex {
    bool operator () (const obj::Index& a, const obj::Index& b) const {
        return a.v == b.v && a.t == b.t && a.n == b.n;
    }
};

template <typename T>
void write_compressed(std::ostream& os, const T* data, int count) {
    const uLongf size = sizeof(T) * count;
    uLongf buf_size = compressBound(size);
    std::unique_ptr<Bytef[]> buf(new Bytef[buf_size]);

    compress(buf.get(), &buf_size, (const Bytef*)data, size);

    os.write((char*)&buf_size, sizeof(uLongf));
    os.write((char*)buf.get(), buf_size);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: zmodel input.obj output.zmod" << std::endl;
        return 1;
    }

    obj::File obj_file;
    std::cout << "Loading mesh..." << std::flush;
    if (!load_obj(Path(argv[1]), obj_file)) {
        std::cout << " error" << std::endl;
        std::cerr << "Cannot load OBJ file \'" << argv[1] << "\'" << std::endl;
        return 1;
    }
    std::cout << " done" << std::endl;

    std::vector<int32_t> indices;
    std::vector<float3>  vertices;
    std::vector<float3>  normals;
    std::vector<float2>  texcoords;
    std::vector<int32_t> materials;

    std::cout << "Rebuilding indices..." << std::flush;
    for (auto& obj: obj_file.objects) {
        // Convert the faces to triangles & build the new list of indices
        std::vector<TriIdx> triangles;
        std::unordered_map<obj::Index, int, HashIndex, CompareIndex> mapping;

        int cur_idx = 0;
        bool has_normals = false;
        bool has_texcoords = false;

        for (auto& group : obj.groups) {
            for (auto& face : group.faces) {
                for (int i = 0; i < face.index_count; i++) {
                    auto map = mapping.find(face.indices[i]);
                    if (map == mapping.end()) {
                        has_normals |= (face.indices[i].n != 0);
                        has_texcoords |= (face.indices[i].t != 0);

                        mapping.insert(std::make_pair(face.indices[i], cur_idx));
                        cur_idx++;
                    }
                }

                const int v0 = mapping[face.indices[0]];
                int prev = mapping[face.indices[1]];
                for (int i = 1; i < face.index_count - 1; i++) {
                    const int next = mapping[face.indices[i + 1]];
                    triangles.push_back(TriIdx(v0, prev, next));
                    materials.push_back(face.material);
                    prev = next;
                }
            }
        }

        if (triangles.size() == 0) continue;

        // Create a mesh for this object        
        int vert_offset = vertices.size();
        int idx_offset = indices.size();
        int vert_count = mapping.size();

        indices.resize(idx_offset + triangles.size() * 3);
        for (auto& t : triangles) {
            indices[idx_offset++] = t.v0 + vert_offset;
            indices[idx_offset++] = t.v1 + vert_offset;
            indices[idx_offset++] = t.v2 + vert_offset;
        }

        vertices.resize(vert_offset + vert_count);
        for (auto& p : mapping) {
            const auto& v = obj_file.vertices[p.first.v];
            vertices[vert_offset + p.second].x = v.x;
            vertices[vert_offset + p.second].y = v.y;
            vertices[vert_offset + p.second].z = v.z;
        }

        texcoords.resize(vert_offset + vert_count);
        if (has_texcoords) {
            for (auto& p : mapping) {
                const auto& t = obj_file.texcoords[p.first.t];
                texcoords[vert_offset + p.second] = t;
            }
        }

        normals.resize(vert_offset + vert_count);
        if (has_normals) {
            for (auto& p : mapping) {
                const auto& n = obj_file.normals[p.first.n];
                normals[vert_offset + p.second] = n;
            }
        }
    }
    std::cout << " done" << std::endl;

    // Write mesh
    std::ofstream out(argv[2], std::ofstream::binary);

    const char header[] = "ZMOD";
    out.write(header, 4);

    int32_t vert_count = vertices.size();
    int32_t tri_count = indices.size() / 3;
    int32_t mtl_count = obj_file.materials.size();

    out.write((char*)&tri_count, sizeof(int32_t));
    out.write((char*)&vert_count, sizeof(int32_t));
    out.write((char*)&mtl_count, sizeof(int32_t));

    std::cout << "Compressing indices..." << std::flush;
    write_compressed(out, indices.data(), 3 * tri_count);
    std::cout << " done" << std::endl;

    std::cout << "Compressing vertices..." << std::flush;
    write_compressed(out, vertices.data(), vert_count);
    std::cout << " done" << std::endl;

    std::cout << "Compressing normals..." << std::flush;
    write_compressed(out, normals.data(), vert_count);
    std::cout << " done" << std::endl;

    std::cout << "Compressing texcoords..." << std::flush;
    write_compressed(out, texcoords.data(), vert_count);
    std::cout << " done" << std::endl;

    std::cout << "Compressing materials..." << std::flush;
    write_compressed(out, materials.data(), tri_count);
    std::cout << " done" << std::endl;

    std::cout << "Writing materials..." << std::flush;
    for (auto& mat: obj_file.materials) {
        int32_t str_len = mat.length();
        out.write((char*)&str_len, sizeof(int32_t));
        out.write(mat.c_str(), str_len);
    }
    std::cout << " done" << std::endl;

    return 0;
}

