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

bool build_scene(const Path& path, Scene& scene) {
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
            id = scene.textures.size();
            tex_map.emplace(name, id);
            scene.textures.emplace_back(new TextureSampler(std::move(img)));
        } else {
            id = -1;
            tex_map.emplace(name, -1);
            printf("fail\n");
        }

        return id;
    };

    // Add a dummy material, for objects that have no material
    scene.materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
    
    for (int i = 1; i < obj_file.materials.size(); i++) {
        auto& mat_name = obj_file.materials[i];
        auto it = mtl_lib.find(mat_name);
        if (it == mtl_lib.end()) {
            // Add a dummy material in this case
            scene.materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
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
                scene.materials.push_back(std::unique_ptr<MirrorMaterial>(new MirrorMaterial()));
            else if (is_emissive) {
                scene.materials.push_back(std::unique_ptr<EmissiveMaterial>(new EmissiveMaterial(float4(mat.ke.x, mat.ke.y, mat.ke.z, 1.0f))));
            } else {
                Material* mtl;
                if (!mat.map_kd.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_kd;
                    
                    int sampler_id = load_texture(img_file);
                    if (sampler_id < 0) {
                        mtl = new LambertMaterial(float4(1.0f, 0.0f, 1.0f, 1.0f)); 
                    } else {
                        mtl = new LambertMaterial(scene.textures[sampler_id].get());
                    }
                } else {
                    mtl = new LambertMaterial(float4(mat.kd.x, mat.kd.y, mat.kd.z, 1.0f));
                }

                // If specified, load the alpha map
                if (!mat.map_d.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_d;
                    
                    int sampler_id = load_texture(img_file);
                    if (sampler_id >= 0) {
                        Material* tmat = new TransparentMaterial();
                        mtl = new CombineMaterial(scene.textures[sampler_id].get(), std::unique_ptr<Material>(mtl), std::unique_ptr<Material>(tmat));
                    }
                }

                scene.materials.push_back(std::unique_ptr<Material>(mtl));
            }
        }
    }

    // Add attributes for texture coordinates and normals
    scene.mesh.add_attribute(Mesh::ATTR_FLOAT2);
    scene.mesh.add_attribute(Mesh::ATTR_FLOAT3);

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
                    scene.material_ids.push_back(face.material);
                    
                    auto mat = scene.materials[face.material].get();
                    if (mat->kind == Material::emissive) {
                        auto p0 = obj_file.vertices[face.indices[0].v];
                        auto p1 = obj_file.vertices[face.indices[i].v];
                        auto p2 = obj_file.vertices[face.indices[i+1].v];
                        
                        // Create a light source for this emissive object.
                        scene.lights.push_back(std::unique_ptr<TriangleLight>(new TriangleLight(static_cast<EmissiveMaterial*>(mat)->color(), 
                            float3(p0.x, p0.y, p0.z), float3(p1.x, p1.y, p1.z), float3(p2.x, p2.y, p2.z))));
                    }
                    
                    prev = next;
                }
            }
        }

        if (triangles.size() == 0) continue;

        // Create a mesh for this object        
        int vert_offset = scene.mesh.vertex_count();
        int offset = scene.mesh.index_count();
        scene.mesh.set_index_count(offset + triangles.size() * 3);
        int i = 0;
        for (TriIdx t : triangles) {
            scene.mesh.indices()[offset + (i++)] = t.v0 + vert_offset;
            scene.mesh.indices()[offset + (i++)] = t.v1 + vert_offset;
            scene.mesh.indices()[offset + (i++)] = t.v2 + vert_offset;
        }

        scene.mesh.set_vertex_count(vert_offset + cur_idx);
        for (auto& p : mapping) {
            const auto& v = obj_file.vertices[p.first.v];
            scene.mesh.vertices()[vert_offset + p.second].x = v.x;
            scene.mesh.vertices()[vert_offset + p.second].y = v.y;
            scene.mesh.vertices()[vert_offset + p.second].z = v.z;
        }

        if (has_texcoords) {
            auto texcoords = scene.mesh.attribute<float2>(MeshAttributes::texcoords);
            // Set up mesh texture coordinates
            for (auto& p : mapping) {
                const auto& t = obj_file.texcoords[p.first.t];
                texcoords[vert_offset + p.second] = t;
            }
        }

        if (has_normals) {
            auto normals = scene.mesh.attribute<float3>(MeshAttributes::normals);
            // Set up mesh normals
            for (auto& p : mapping) {
                const auto& n = obj_file.normals[p.first.n];
                normals[vert_offset + p.second] = n;
            }
        } else {
            // Recompute normals
            scene.mesh.compute_normals(true, MeshAttributes::normals);
        }
    }

    return true;
}

} // namespace imba
