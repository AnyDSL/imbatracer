#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <memory>
#include "build_scene.h"
#include "../loaders/loaders.h"

namespace imba {

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

bool build_scene(const Path& path, Mesh& scene, MaterialContainer& scene_materials, TextureContainer& textures,
                 std::vector<int>& triangle_material_ids, std::vector<float2>& texcoords, LightContainer& lights) {
    obj::File obj_file;
    if(!load_obj(path, obj_file)) {
        return false;
    }

    // Parse the associated MTL files
    obj::MaterialLib mtl_lib;
    for (auto& lib : obj_file.mtl_libs) {
        if (!load_mtl(path.base_name() + "/" + lib, mtl_lib)) {
            return false;
        }
    }

    std::unordered_map<std::string, int> tex_map;
    auto load_texture = [&](const std::string& name) {
        auto tex = tex_map.find(name);
        if (tex != tex_map.end())
            return tex->second;

        Image img;
        int id;
        if (load_image(name, img)) {
            id = textures.size();
            tex_map.emplace(name, id);
            textures.emplace_back(new TextureSampler(std::move(img)));
        } else {
            id = -1;
            tex_map.emplace(name, -1);
        }

        return id;
    };

    // Add a dummy material, for objects that have no material
    scene_materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
    
    for (int i = 1; i < obj_file.materials.size(); i++) {
        auto& mat_name = obj_file.materials[i];
        auto it = mtl_lib.find(mat_name);
        if (it == mtl_lib.end()) {
            // Add a dummy material in this case
            scene_materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
        } else {
            const obj::Material& mat = it->second;

            // Change the ambient map if needed
            std::string map_ka;
            if (mat.map_ka.empty() &&
                dot(mat.ka, mat.ka) > 0.0f &&
                !mat.map_kd.empty()) {
                map_ka = mat.map_kd;
            } else {
                map_ka = mat.map_ka;
            }
            
            bool is_emissive = !mat.map_ke.empty() || (mat.ke.x > 0.0f && mat.ke.y > 0.0f && mat.ke.z > 0.0f);

            if (mat.illum == 5)
                scene_materials.push_back(std::unique_ptr<MirrorMaterial>(new MirrorMaterial()));
            else if (is_emissive) {
                scene_materials.push_back(std::unique_ptr<EmissiveMaterial>(new EmissiveMaterial(float4(mat.ke.x, mat.ke.y, mat.ke.z, 1.0f))));
            } else {
                LambertMaterial* mtl;
                if (!mat.map_kd.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_kd;
                    
                    int sampler_id = load_texture(img_file);
                    if (sampler_id < 0) {
                        mtl = new LambertMaterial(float4(1.0f, 0.0f, 1.0f, 1.0f)); 
                    } else {
                        mtl = new LambertMaterial(textures[sampler_id].get());
                    }
                } else {
                    mtl = new LambertMaterial(float4(mat.kd.x, mat.kd.y, mat.kd.z, 1.0f));
                }
               
                scene_materials.push_back(std::unique_ptr<LambertMaterial>(mtl));
            }
        }
    }

    // Create a scene from the OBJ file.
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
                    triangle_material_ids.push_back(face.material);
                    
                    auto mat = scene_materials[face.material].get();
                    if (mat->kind == Material::emissive) {
                        auto p0 = obj_file.vertices[face.indices[0].v];
                        auto p1 = obj_file.vertices[face.indices[i].v];
                        auto p2 = obj_file.vertices[face.indices[i+1].v];
                        
                        // Create a light source for this emissive object.
                        lights.push_back(std::unique_ptr<TriangleLight>(new TriangleLight(static_cast<EmissiveMaterial*>(mat)->color(), 
                            float3(p0.x, p0.y, p0.z), float3(p1.x, p1.y, p1.z), float3(p2.x, p2.y, p2.z))));
                    }
                    
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
            const auto& v = obj_file.vertices[p.first.v];
            scene.vertices()[vert_offset + p.second].x = v.x;
            scene.vertices()[vert_offset + p.second].y = v.y;
            scene.vertices()[vert_offset + p.second].z = v.z;
        }

        if (has_texcoords) {
            // Set up mesh texture coordinates
            texcoords.resize(vert_offset + cur_idx);
            scene.set_texcoord_count(vert_offset + cur_idx);
            for (auto& p : mapping) {
                const auto& t = obj_file.texcoords[p.first.t];
                texcoords[vert_offset + p.second] = t;
                scene.texcoords()[vert_offset + p.second] = t;
            }
        }

 /*       if (has_normals) {
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

} // namespace imba
