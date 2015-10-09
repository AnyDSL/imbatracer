#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <memory>
#include "obj_loader.h"

namespace imba {

struct TriIdx {
    int v0, v1, v2;
    TriIdx(int v0, int v1, int v2) : v1(v1), v2(v2), v0(v0) { }
};


bool ObjLoader::check_format(const Path& path) {
    // Checks only the file extension
    return path.extension() == "obj";
}

bool ObjLoader::load_file(const Path& path, Mesh& scene, MaterialContainer& scene_materials, std::vector<int>& triangle_material_ids, Logger* logger) {
    ObjFile obj_file;

    // Parse the OBJ file
    {
        std::ifstream stream(path);
        if (!stream || !parse_obj_stream(stream, obj_file, logger)) {
            return false;
        }
    }

    // Parse the associated MTL files
    std::unordered_map<std::string, ObjMaterial> materials;
    for (auto& lib : obj_file.mtl_libs) {
        std::ifstream stream(path.base_name() + "/" + lib);
        if (!stream || !parse_mtl_stream(stream, materials, logger)) {
            if (logger) logger->log("invalid material library '", lib, "'");
        }
    }

    // Add a dummy material, for objects that have no material
    scene_materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
    
    // Add all the other materials
    /*std::unordered_map<std::string, TextureId> tex_map;
    auto load_map = [this, &tex_map, &logger, &scene, path] (const std::string& name, bool gs) {
        TextureId tex_id;
        if (name.empty()) return tex_id;

        if (tex_map.find(name) == tex_map.end()) {
            Texture tex;
            if (load_texture(Path(path.base_name() + "/" + name), tex, logger)) {
                tex_id = scene.new_texture(std::move(gs ? grayscale(tex) : std::move(tex)));
                auto tex = scene.texture(tex_id);
                if (is_pow2(tex->width()) && is_pow2(tex->height())) {
                    generate_mipmaps(*tex.get());
                }
            }
            tex_map[name] = tex_id;
        } else {
            tex_id = tex_map[name];
        }
        return tex_id;
    };*/

    for (int i = 1; i < obj_file.materials.size(); i++) {
        auto& mat_name = obj_file.materials[i];
        auto it = materials.find(mat_name);
        if (it == materials.end()) {
            if (logger) logger->log("material not found '", mat_name, "'");
            // Add a dummy material in this case
            scene_materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
        } else {
            const ObjMaterial& mat = it->second;

            // Change the ambient map if needed
            std::string map_ka;
            if (mat.map_ka.empty() &&
                dot(mat.ka, mat.ka) > 0.0f &&
                !mat.map_kd.empty()) {
                map_ka = mat.map_kd;
            } else {
                map_ka = mat.map_ka;
            }

            if (mat.illum == 5)
                scene_materials.push_back(std::unique_ptr<MirrorMaterial>(new MirrorMaterial()));
            else
                scene_materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial(float4(mat.kd.x, mat.kd.y, mat.kd.z, 1.0f))));
        }
    }

    // Create a scene from the OBJ file.
    for (auto& obj: obj_file.objects) {
        // Convert the faces to triangles & build the new list of indices
        std::vector<TriIdx> triangles;

        auto hash_index = [] (const Index& i) { return i.v ^ (i.t << 7) ^ (i.n << 11); };
        auto pred_index = [] (const Index& a, const Index& b) { return (a.v == b.v) && (a.n == b.n) && (a.t == b.t); };
        std::unordered_map<Index, int, decltype(hash_index), decltype(pred_index)> mapping(obj_file.vertices.size(), hash_index, pred_index);

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
                    triangle_material_ids.push_back(face.material);
                    prev = next;
                }
            }
        }

        if (triangles.size() == 0) continue;

        // Create a mesh for this object        
        int vert_offset = scene.vertex_count();
        int offset = scene.index_count();
        scene.set_index_count(offset + triangles.size() * 3);
        int i = 0;
        for (TriIdx t : triangles) {
            scene.indices()[offset + (i++)] = t.v0 + vert_offset;
            scene.indices()[offset + (i++)] = t.v1 + vert_offset;
            scene.indices()[offset + (i++)] = t.v2 + vert_offset;
        }

        scene.set_vertex_count(vert_offset + cur_idx);
        for (auto& p : mapping) {
            const Vertex& v = obj_file.vertices[p.first.v];
            scene.vertices()[vert_offset + p.second].x = v.x;
            scene.vertices()[vert_offset + p.second].y = v.y;
            scene.vertices()[vert_offset + p.second].z = v.z;
        }

       /* if (has_texcoords) {
            // Set up mesh texture coordinates
            mesh->set_texcoord_count(cur_idx);
            for (auto& p : mapping) {
                const Texcoord& t = obj_file.texcoords[p.first.t];
                mesh->texcoords()[p.second] = Vec2(t.u, t.v);
            }
        }

        if (has_normals) {
            // Set up mesh normals
            mesh->set_normal_count(cur_idx);
            for (auto& p : mapping) {
                const Normal& n = obj_file.normals[p.first.n];
                mesh->normals()[p.second] = float3(n.x, n.y, n.z);
            }
        } else {
            // Recompute normals
            if (logger) logger->log("Recomputing normals...");
            mesh->compute_normals(true);
        }*/
/*
        if (logger) {
            logger->log("mesh with ", mesh->vertex_count(), " vertices, ",
                                      mesh->triangle_count(), " triangles");
        }
*/
        /*for (int z = 0; z < 50; z++) {
            for (int y = 0; y < 50; y++) {
                for (int x = 0; x < 50; x++) {
                    scene.new_instance(mesh_id, imba::Mat4::translation(imba::float3(-1250 + x * 50, -1250 + y * 50, -1250 + z * 50)));
                }
            }
        }
        scene.new_instance(mesh_id);*/
    }

    return true;
}

inline char* strip_spaces(char* ptr) {
    while (std::isspace(*ptr)) { ptr++; }
    return ptr;
}

inline char* strip_text(char* ptr) {
    while (*ptr && !std::isspace(*ptr)) { ptr++; }
    return ptr;
}

inline void remove_eol(char* ptr) {
    int i = std::strlen(ptr) - 1;
    while (i > 0 && std::isspace(ptr[i])) {
        ptr[i] = '\0';
        i--;
    }
}

bool ObjLoader::parse_obj_stream(std::istream& stream, ObjFile& file, Logger* logger) {
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

    int smooth = 0;
    const int max_line = 1024;
    char line[max_line];
    while (stream.getline(line, max_line)) {
        // Strip spaces
        char* ptr = strip_spaces(line);
        const char* err_line = ptr;

        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;

        remove_eol(ptr);

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
                    if (logger) logger->log("unknown command '", err_line, "'");
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
                if (logger) logger->log("face with less than 3 vertices '", err_line, "'");
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
                    if (f.indices[i].v <= 0 || f.indices[i].t < 0 || f.indices[i].n < 0) {
                        valid = false;
                        break;
                    }
                }

                if (valid) {
                    file.objects[cur_object].groups[cur_group].faces.push_back(f);
                } else {
                    if (logger) logger->log("invalid face indices '", err_line, "'");
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

            ptr = strip_spaces(ptr);
            char* base = ptr;
            ptr = strip_text(ptr);

            const std::string mtl_name(base, ptr);

            cur_mtl = std::find(file.materials.begin(), file.materials.end(), mtl_name) - file.materials.begin();
            if (cur_mtl == (int)file.materials.size()) {
                file.materials.push_back(mtl_name);            
            }
        } else if (!std::strncmp(ptr, "mtllib", 6) && std::isspace(ptr[6])) {
            ptr += 6;

            ptr = strip_spaces(ptr);
            char* base = ptr;
            ptr = strip_text(ptr);

            const std::string lib_name(base, ptr);

            file.mtl_libs.push_back(lib_name);
        } else if (*ptr == 's' && std::isspace(ptr[1])) {
            smooth++;
        } else {
            if (logger) logger->log("unknown command '", err_line, "'");
        }
    }

    if (smooth && logger) logger->log(smooth, " smooth command(s) ignored");

    return true;
}

bool ObjLoader::parse_mtl_stream(std::istream& stream, std::unordered_map<std::string, ObjMaterial>& materials, Logger* logger) {
    const int max_line = 1024;
    char line[max_line];
    char* err_line = line;

    std::string mtl_name;
    auto current_material = [&] () -> ObjMaterial& {
        if (!mtl_name.length() && logger)
            logger->log("invalid material command '", err_line, "'");
        return materials[mtl_name];
    };

    while (stream.getline(line, max_line)) {
        // Strip spaces
        char* ptr = strip_spaces(line);
        err_line = ptr;

        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;

        remove_eol(ptr);

        if (!std::strncmp(ptr, "newmtl", 6) && std::isspace(ptr[6])) {
            ptr = strip_spaces(ptr + 7);
            char* base = ptr;
            ptr = strip_text(ptr);

            mtl_name = std::string(base, ptr);
            if (materials.find(mtl_name) != materials.end()) {
                if (logger) logger->log("duplicate material name '", mtl_name, "'");
            }
        } else if (ptr[0] == 'K') {
            if (ptr[1] == 'a' && std::isspace(ptr[2])) {
                ObjMaterial& mat = current_material();
                mat.ka[0] = std::strtof(ptr + 3, &ptr);
                mat.ka[1] = std::strtof(ptr, &ptr);
                mat.ka[2] = std::strtof(ptr, &ptr);
            } else if (ptr[1] == 'd' && std::isspace(ptr[2])) {
                ObjMaterial& mat = current_material();
                mat.kd[0] = std::strtof(ptr + 3, &ptr);
                mat.kd[1] = std::strtof(ptr, &ptr);
                mat.kd[2] = std::strtof(ptr, &ptr);
            } else if (ptr[1] == 's' && std::isspace(ptr[2])) {
                ObjMaterial& mat = current_material();
                mat.ks[0] = std::strtof(ptr + 3, &ptr);
                mat.ks[1] = std::strtof(ptr, &ptr);
                mat.ks[2] = std::strtof(ptr, &ptr);
            }
        } else if (ptr[0] == 'N' && ptr[1] == 's' && std::isspace(ptr[2])) {
            ObjMaterial& mat = current_material();
            mat.ns = std::strtof(ptr + 3, &ptr);
        } else if (ptr[0] == 'd' && std::isspace(ptr[1])) {
            ObjMaterial& mat = current_material();
            mat.d = std::strtof(ptr + 2, &ptr);
        } else if (!std::strncmp(ptr, "illum", 5) && std::isspace(ptr[5])) {
            ObjMaterial& mat = current_material();
            mat.illum = std::strtol(ptr + 6, &ptr, 10);
        } else if (!std::strncmp(ptr, "map_Ka", 6) && std::isspace(ptr[6])) {
            ObjMaterial& mat = current_material();
            mat.map_ka = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_Kd", 6) && std::isspace(ptr[6])) {
            ObjMaterial& mat = current_material();
            mat.map_kd = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_Ks", 6) && std::isspace(ptr[6])) {
            ObjMaterial& mat = current_material();
            mat.map_ks = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_bump", 8) && std::isspace(ptr[8])) {
            ObjMaterial& mat = current_material();
            mat.map_bump = std::string(strip_spaces(ptr + 9));
        } else if (!std::strncmp(ptr, "bump", 4) && std::isspace(ptr[4])) {
            ObjMaterial& mat = current_material();
            mat.map_bump = std::string(strip_spaces(ptr + 5));
        } else if (!std::strncmp(ptr, "map_d", 5) && std::isspace(ptr[5])) {
            ObjMaterial& mat = current_material();
            mat.map_d = std::string(strip_spaces(ptr + 6));
        } else {
            if (logger) logger->log("unknown material command '", err_line, "'");
        }
    }

    return true;
}

bool ObjLoader::read_index(char** ptr, ObjLoader::Index& idx) {
    char* base = *ptr;

    // Detect end of line (negative indices are supported) 
    base = strip_spaces(base);
    if (!std::isdigit(*base) && *base != '-') return false;

    idx.v = 0;
    idx.t = 0;
    idx.n = 0;

    idx.v = std::strtol(base, &base, 10);

    base = strip_spaces(base);

    if (*base == '/') {
        base++;

        // Handle the case when there is no texture coordinate
        if (*base != '/') {
            idx.t = std::strtol(base, &base, 10);
        }

        base = strip_spaces(base);

        if (*base == '/') {
            base++;
            idx.n = std::strtol(base, &base, 10);
        }
    }

    *ptr = base;

    return true;
}

} // namespace imba
