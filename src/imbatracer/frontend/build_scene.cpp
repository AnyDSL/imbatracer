#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <memory>

#include "build_scene.h"
#include "../loaders/loaders.h"
#include "../core/adapter.h"

namespace imba {

struct TriIdx {
    int v0, v1, v2, m;
    TriIdx(int v0, int v1, int v2, int m)
        : v1(v1), v2(v2), v0(v0), m(m)
    {}
};

struct HashIndex {
    size_t operator () (const obj::Index& i) const {
        unsigned h = 0, g;

        h = (h << 4) + i.v;
        g = h & 0xF0000000;
        h = g ? (h ^ (g >> 24)) : h;
        h &= ~g;

        h = (h << 4) + i.t;
        g = h & 0xF0000000;
        h = g ? (h ^ (g >> 24)) : h;
        h &= ~g;

        h = (h << 4) + i.n;
        g = h & 0xF0000000;
        h = g ? (h ^ (g >> 24)) : h;
        h &= ~g;

        return h;
    }
};

struct CompareIndex {
    bool operator () (const obj::Index& a, const obj::Index& b) const {
        return a.v == b.v && a.t == b.t && a.n == b.n;
    }
};

void convert_materials(const Path& path, const obj::File& obj_file, const obj::MaterialLib& mtl_lib, Scene& scene) {
    MaskBuffer masks;

    std::unordered_map<std::string, int> tex_map;
    auto load_texture = [&](const std::string& name) {
        auto tex = tex_map.find(name);
        if (tex != tex_map.end())
            return tex->second;

        std::cout << "  Loading texture " << name << "..." << std::flush;

        Image img;
        int id;
        if (load_image(name, img)) {
            id = scene.textures.size();
            tex_map.emplace(name, id);
            scene.textures.emplace_back(new TextureSampler(std::move(img)));
            std::cout << std::endl;
        } else {
            id = -1;
            tex_map.emplace(name, -1);
            std::cout << " FAILED!" << std::endl;
        }

        return id;
    };

    std::unordered_map<int, int> mask_map;

    // Add a dummy material, for objects that have no material
    scene.materials.push_back(std::unique_ptr<LambertMaterial>(new LambertMaterial));
    masks.add_desc();

    for (int i = 1; i < obj_file.materials.size(); i++) {
        auto& mat_name = obj_file.materials[i];
        auto it = mtl_lib.find(mat_name);

        int mask_id = -1;
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
                scene.materials.push_back(std::unique_ptr<MirrorMaterial>(new MirrorMaterial(1.0f, mat.ns, mat.ks)));
            else if (mat.illum == 7 /* HACK !!!  || mat.ni != 0*/)
                scene.materials.push_back(std::unique_ptr<GlassMaterial>(new GlassMaterial(mat.ni, /* HACK !!! */ mat.tf /*mat.kd*/, mat.ks)));
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

                scene.materials.push_back(std::unique_ptr<Material>(mtl));
            }

            // If specified, load the alpha map
            if (!mat.map_d.empty()) {
                mask_id = load_texture(path.base_name() + "/" + mat.map_d);
            }
        }

        if (mask_id >= 0) {
            auto offset = mask_map.find(mask_id);
            const auto& image = scene.textures[mask_id]->image();
            if (offset != mask_map.end()) {
                masks.add_desc(MaskBuffer::MaskDesc(image.width(), image.height(), offset->second));
            } else {
                auto desc = masks.append_mask(image);
                mask_map.emplace(mask_id, desc.offset);
            }
        } else {
            masks.add_desc();
        }
    }

    // Send the masks to the GPU
    scene.masks = std::move(ThorinArray<::TransparencyMask>(masks.mask_count()));
    memcpy(scene.masks.begin(), masks.descs(), sizeof(MaskBuffer::MaskDesc) * masks.mask_count());
    scene.mask_buffer = std::move(ThorinArray<char>(masks.buffer_size()));
    memcpy(scene.mask_buffer.begin(), masks.buffer(), masks.buffer_size());
}

void create_mesh(const obj::File& obj_file, Scene& scene) {
    // Add attributes for texture coordinates and normals
    scene.mesh.add_attribute(Mesh::ATTR_FLOAT2);
    scene.mesh.add_attribute(Mesh::ATTR_FLOAT3);

    for (auto& obj: obj_file.objects) {
        // Convert the faces to triangles & build the new list of indices
        std::vector<TriIdx> triangles;
        std::unordered_map<obj::Index, int, HashIndex, CompareIndex> mapping;

        bool has_normals = false;
        bool has_texcoords = false;
        for (auto& group : obj.groups) {
            for (auto& face : group.faces) {
                for (int i = 0; i < face.index_count; i++) {
                    auto map = mapping.find(face.indices[i]);
                    if (map == mapping.end()) {
                        has_normals |= (face.indices[i].n != 0);
                        has_texcoords |= (face.indices[i].t != 0);

                        mapping.insert(std::make_pair(face.indices[i], mapping.size()));
                    }
                }

                const int v0 = mapping[face.indices[0]];
                int prev = mapping[face.indices[1]];
                for (int i = 1; i < face.index_count - 1; i++) {
                    const int next = mapping[face.indices[i + 1]];
                    triangles.emplace_back(v0, prev, next, face.material);

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
        int idx_offset = scene.mesh.index_count();
        scene.mesh.set_index_count(idx_offset + triangles.size() * 4);
        for (TriIdx t : triangles) {
            scene.mesh.indices()[idx_offset++] = t.v0 + vert_offset;
            scene.mesh.indices()[idx_offset++] = t.v1 + vert_offset;
            scene.mesh.indices()[idx_offset++] = t.v2 + vert_offset;
            scene.mesh.indices()[idx_offset++] = t.m;
        }

        scene.mesh.set_vertex_count(vert_offset + mapping.size());

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
            std::cout << "  Recomputing normals..." << std::flush;
            scene.mesh.compute_normals(true, MeshAttributes::normals);
            std::cout << std::endl;
        }
    }
}

bool build_scene(const Path& path, Scene& scene) {
    obj::File obj_file;

    std::cout << "[1/7] Loading OBJ..." << std::flush;
    if (!load_obj(path, obj_file)) {
        std::cout << " FAILED" << std::endl;
        return false;
    }
    std::cout << std::endl;

    obj::MaterialLib mtl_lib;

    // Parse the associated MTL files
    std::cout << "[2/7] Loading MTLs..." << std::flush;
    for (auto& lib : obj_file.mtl_libs) {
        if (!load_mtl(path.base_name() + "/" + lib, mtl_lib)) {
            std::cout << " FAILED" << std::endl;
            return false;
        }
    }
    std::cout << std::endl;

    std::cout << "[3/7] Converting materials..." << std::endl;
    convert_materials(path, obj_file, mtl_lib, scene);

    // Create a scene from the OBJ file.
    std::cout << "[4/7] Creating scene..." << std::endl;
    create_mesh(obj_file, scene);

    // Check for invalid normals
    std::cout << "[5/7] Validating scene..." << std::endl;

    bool bad_normals = false;
    auto normals = scene.mesh.attribute<float3>(MeshAttributes::normals);
    for (int i = 0; i < scene.mesh.vertex_count(); i++) {
        auto& n = normals[i];
        if (isnan(n.x) || isnan(n.y) || isnan(n.z)) {
            n.x = 0;
            n.y = 1;
            n.z = 0;
            bad_normals = true;
        }
    }

    if (bad_normals) std::cout << "  Normals containing invalid values have been replaced" << std::endl;

    if (scene.mesh.triangle_count() == 0) {
        std::cout << " There is no triangle in the scene." << std::endl;
        return false;
    }

    //scene.lights.emplace_back(new DirectionalLight(normalize(float3(0.7f, -1.0f, -1.001f)), float4(2.5f)));
    //scene.lights.emplace_back(new PointLight(float3(-10.0f, 193.f, -4.5f), float4(100000.5f)));
    //scene.lights.emplace_back(new PointLight(float3(9.0f, 3.0f, 6.0f), float4(500.0f)));
    //scene.lights.emplace_back(new PointLight(float3(0.0f, 0.8f, 1.0f), float4(200.0f)));

    if (scene.lights.empty()) {
        std::cout << "  There are no lights in the scene." << std::endl;
        return false;
    }

    // Compute geometry normals
    scene.geom_normals.resize(scene.mesh.triangle_count());
    for (int i = 0; i < scene.mesh.triangle_count(); ++i) {
        auto t = scene.mesh.triangle(i);
        scene.geom_normals[i] = normalize(cross(t[1] - t[0], t[2] - t[0]));
    }

    scene.texcoords = std::move(ThorinArray<::Vec2>(scene.mesh.vertex_count()));
    {
        auto texcoords = scene.mesh.attribute<float2>(MeshAttributes::texcoords);
        for (int i = 0; i < scene.mesh.vertex_count(); i++) {
            scene.texcoords[i].x = texcoords[i].x;
            scene.texcoords[i].y = texcoords[i].y;
        }
    }

    scene.indices = std::move(ThorinArray<int>(scene.mesh.index_count()));
    {
        for (int i = 0; i < scene.mesh.index_count(); i++)
            scene.indices[i] = scene.mesh.indices()[i];
    }

    std::cout << "[6/7] Building acceleration structure..." << std::endl;
    {
        std::vector<::Node> nodes;
        std::vector<::Vec4> tris;
        std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
        adapter->build_accel(scene.mesh);
#ifdef STATISTICS
        std::cout << "  "; adapter->print_stats();
#endif
        scene.nodes = nodes;
        scene.tris = tris;
    }

    std::cout << "[7/7] Moving the scene to the device..." << std::flush;
    scene.nodes.upload();
    scene.tris.upload();
    scene.masks.upload();
    scene.mask_buffer.upload();
    scene.indices.upload();
    scene.texcoords.upload();
    std::cout << std::endl;

    return true;
}

} // namespace imba
