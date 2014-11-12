#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <unordered_map>
#include "obj_loader.hpp"

namespace imba {

bool ObjLoader::load_scene(const std::string& working_dir,
                           const std::string& name, Scene& scene,
                           Logger* logger) {
    File obj_file;
    // Parse the OBJ file
    {
        std::ifstream stream(working_dir + "//" + name);
        if (!stream || !parse_stream(stream, obj_file, logger)) {
            return false;
        }
    }

    // Create a scene from the OBJ file.
    for (auto& obj: obj_file.objects) {
        // Create a mesh for this object
        std::unique_ptr<TriangleMesh> mesh(new TriangleMesh());
        
        auto hash_index = [] (const Index& i) { return i.v ^ (i.t << 7) ^ (i.n << 11); };
        auto pred_index = [] (const Index& a, const Index& b) { return (a.v == b.v) && (a.n == b.n) && (a.t == b.t); };
        std::unordered_map<Index, int, decltype(hash_index), decltype(pred_index)> mapping(obj_file.vertices.size(), hash_index, pred_index);

        int cur_idx = 0;
        bool has_normals = false;
        bool has_texcoords = false;
        int tri_count = 0;

        // Convert the faces to triangles & build the new list of indices
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
                    mesh->add_triangle(TriangleMesh::Triangle(v0, prev, next));
                    mesh->add_material(face.material);
                    tri_count++;
                    prev = next;
                }
            }
        }

        if (tri_count == 0) continue;

        // Set up mesh vertices
        mesh->set_vertex_count(cur_idx);
        for (auto& p : mapping) {
            const Vertex& v = obj_file.vertices[p.first.v];
            mesh->vertices()[p.second] = Vec3(v.x, v.y, v.z);
        }

        if (has_normals) {
            // Set up mesh normals
            mesh->set_normal_count(cur_idx);
            for (auto& p : mapping) {
                const Normal& n = obj_file.normals[p.first.n];
                mesh->normals()[p.second] = Vec3(n.x, n.y, n.z);
            }
        }

        if (has_texcoords) {
            // Set up mesh texture coordinates
            mesh->set_texcoord_count(cur_idx);
            for (auto& p : mapping) {
                const Texcoord& t = obj_file.texcoords[p.first.t];
                mesh->texcoords()[p.second] = Vec2(t.u, t.v);
            }
        }

        if (logger) {
            logger->log("mesh with ", mesh->vertex_count(), " vertices, ",
                                      mesh->triangle_count(), " triangles");
        }

        scene.add_triangle_mesh(mesh.release());
    }

    return true;
}

bool ObjLoader::parse_stream(std::istream& stream, File& file, Logger* logger) {
    // Add an empty object to the scene
    int cur_object = 0;
    file.objects.emplace_back();

    // Add an empty group to this object
    int cur_group = 0;
    file.objects[0].groups.emplace_back();

    // Add an empty material to the scene
    int cur_mtl = 0;
    file.materials.emplace_back("");

    // Add dummy vertex, normal, and texcoord
    file.vertices.emplace_back();
    file.normals.emplace_back();
    file.texcoords.emplace_back();

    const int max_line = 1024;
    char line[max_line];
    while (stream.getline(line, max_line)) {
        // Strip spaces
        char* ptr = line;
        while (std::isspace(*ptr)) { ptr++; }

        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;

        // Test each command in turn, the most frequent first
        if (*ptr == 'v') {
            switch (ptr[1]) {
                case ' ':
                case '\t':
                    {
                        Vertex v;
                        v.x = std::strtof(ptr + 1, &ptr);
                        v.y = std::strtof(ptr, &ptr);
                        v.z = std::strtof(ptr, &ptr);
                        file.vertices.push_back(v);
                    }
                    break;
                case 'n':
                    {
                        Normal n;
                        n.x = std::strtof(ptr + 2, &ptr);
                        n.y = std::strtof(ptr, &ptr);
                        n.z = std::strtof(ptr, &ptr);
                        file.normals.push_back(n);
                    }
                    break;
                case 't':
                    {
                        Texcoord t;
                        t.u = std::strtof(ptr + 2, &ptr);
                        t.v = std::strtof(ptr, &ptr);
                        file.texcoords.push_back(t);
                    }
                    break;
                default:
                    if (logger) logger->log("unknown command '", line, "'");
                    break;
            }
        } else if (*ptr == 'f' && std::isspace(ptr[1])) {
            Face f;

            f.index_count = 0;
            f.material = cur_mtl;

            bool valid = true;
            ptr += 2;
            while(f.index_count < Face::max_indices) {
                Index index;
                valid = read_index(&ptr, index);

                if (valid) {
                    f.indices[f.index_count++] = index;
                } else {
                    break;
                }
            }

            if (f.index_count < 3) {
                if (logger) logger->log("face with less than 3 vertices '", line, "'");
            } else {
                // Convert relative indices to absolute
                for (int i = 0; i < f.index_count; i++) {
                    f.indices[i].v = (f.indices[i].v < 0) ? file.vertices.size()  + f.indices[i].v : f.indices[i].v;
                    f.indices[i].t = (f.indices[i].t < 0) ? file.texcoords.size() + f.indices[i].t : f.indices[i].t;
                    f.indices[i].n = (f.indices[i].n < 0) ? file.normals.size()   + f.indices[i].n : f.indices[i].n;
                }

                // Check if the indices are valid or not
                valid = true;
                for (int i = 0; i < f.index_count; i++) {
                    if (f.indices[i].v < 0 || f.indices[i].t < 0 || f.indices[i].n < 0) {
                        valid = false;
                        break;
                    }
                }

                if (valid) {
                    file.objects[cur_object].groups[cur_group].faces.push_back(f);
                } else {
                    if (logger) logger->log("invalid face indices '", line, "'");
                }
            }
        } else if (*ptr == 'g' && std::isspace(ptr[1])) {
            file.objects[cur_object].groups.emplace_back();
            cur_group++;
        } else if (*ptr == 'o' && std::isspace(ptr[1])) {
            file.objects.emplace_back();
            cur_object++;

            file.objects[cur_object].groups.emplace_back();
            cur_group = 0;
        } else if (!std::strncmp(ptr, "usemtl", 6) && std::isspace(ptr[6])) {
            ptr += 6;

            while (std::isspace(*ptr)) { ptr++; }
            char* base = ptr;
            while (*ptr && !std::isspace(*ptr)) { ptr++; }

            const std::string mtl_name(base, ptr);

            cur_mtl = std::find(file.materials.begin(), file.materials.end(), mtl_name) - file.materials.begin();
            if (cur_mtl == (int)file.materials.size()) {
                file.materials.push_back(mtl_name);            
            }
        } else if (!std::strncmp(ptr, "mtllib", 6) && std::isspace(ptr[6])) {
            ptr += 6;

            while (std::isspace(*ptr)) { ptr++; }
            char* base = ptr;
            while (*ptr && !std::isspace(*ptr)) { ptr++; }

            const std::string lib_name(base, ptr);

            file.mtl_libs.push_back(lib_name);
        } else if (*ptr == 's' && std::isspace(ptr[1])) {
            if (logger) logger->log("smooth command ignored '", line, "'");
        } else {
            if (logger) logger->log("unknown command '", line, "'");
        }
    }

    return true;
}

bool ObjLoader::read_index(char** ptr, ObjLoader::Index& idx) {
    char* base = *ptr;

    // Detect end of line (negative indices are supported) 
    while (isspace(*base)) { base++; }
    if (!std::isdigit(*base) && *base != '-') return false;

    idx.v = 0;
    idx.t = 0;
    idx.n = 0;

    idx.v = std::strtol(base, &base, 10);

    // Strip spaces
    while (std::isspace(*base)) { base++; }

    if (*base == '/') {
        base++;

        // Handle the case when there is no texture coordinate
        if (*base != '/') {
            idx.t = std::strtol(base, &base, 10);
        }

        // Strip spaces
        while (std::isspace(*base)) { base++; }

        if (*base == '/') {
            base++;
            idx.n = std::strtol(base, &base, 10);
        }
    }

    *ptr = base;

    return true;
}

} // namespace imba

