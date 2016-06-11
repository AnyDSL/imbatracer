#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <memory>
#include <fstream>

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

using MtlLightBuffer = std::unordered_map<int, float4>;

void convert_materials(const Path& path, const obj::File& obj_file, const obj::MaterialLib& mtl_lib, Scene& scene, MtlLightBuffer& mtl_to_light_intensity) {
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
            id = scene.texture_count();
            tex_map.emplace(name, id);
            scene.textures().emplace_back(new TextureSampler(std::move(img)));
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
    scene.materials().emplace_back(new DiffuseMaterial);
    masks.add_desc();

    for (int i = 1; i < obj_file.materials.size(); i++) {
        auto& mat_name = obj_file.materials[i];
        auto it = mtl_lib.find(mat_name);

        int mask_id = -1;
        if (it == mtl_lib.end()) {
            // Add a dummy material in this case
            scene.materials().emplace_back(new DiffuseMaterial);
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

            // We do not support textured light sources yet.
            bool is_emissive = /*!mat.map_ke.empty() || */(mat.ke.x > 0.0f || mat.ke.y > 0.0f || mat.ke.z > 0.0f);

            bool is_phong = /*!mat.map_ks.empty() ||*/(mat.ks.x > 0.0f || mat.ks.y > 0.0f || mat.ks.z > 0.0f);

            if (is_emissive)
                mtl_to_light_intensity.insert(std::make_pair(scene.material_count(), float4(mat.ke, 1.0f)));

            TextureSampler* bump_sampler = nullptr;
            if (!mat.map_bump.empty()) {
                // Load the bump map.
                const std::string img_file = path.base_name() + "/" + mat.map_bump;
                int sampler_id = load_texture(img_file);
                if (sampler_id >= 0)
                    bump_sampler = scene.texture(sampler_id).get();
            }

            if (mat.illum == 5)
                scene.materials().emplace_back(new MirrorMaterial(1.0f, mat.ns, float4(mat.ks, 1.0f), bump_sampler));
            else if (mat.illum == 7) ///* HACK !!! */ || mat.ni != 0)
                scene.materials().emplace_back(new GlassMaterial(mat.ni, /* HACK !!! mat.kd*/ float4(mat.tf, 1.0f), float4(mat.ks, 1.0f), bump_sampler));
            else if (is_phong){
                Material* mtl;
                if (!mat.map_kd.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_kd;

                    int sampler_id = load_texture(img_file);
                    if (sampler_id < 0) {
                        mtl = new GlossyMaterial(mat.ns, float4(mat.ks, 1.0f), float4(1.0f, 0.0f, 1.0f, 1.0f), bump_sampler);
                    } else {
                        mtl = new GlossyMaterial(mat.ns, float4(mat.ks, 1.0f), scene.texture(sampler_id).get(), bump_sampler);
                    }
                } else {
                    mtl = new GlossyMaterial(mat.ns, float4(mat.ks, 1.0f), float4(mat.kd, 1.0f), bump_sampler);
                }

                scene.materials().emplace_back(mtl);
            } else {
                Material* mtl;
                if (!mat.map_kd.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_kd;

                    int sampler_id = load_texture(img_file);
                    if (sampler_id < 0) {
                        mtl = new DiffuseMaterial(float4(1.0f, 0.0f, 1.0f, 1.0f), bump_sampler);
                    } else {
                        mtl = new DiffuseMaterial(scene.texture(sampler_id).get(), bump_sampler);
                    }
                } else {
                    mtl = new DiffuseMaterial(float4(mat.kd, 1.0f), bump_sampler);
                }

                scene.materials().emplace_back(mtl);
            }

            // If specified, load the alpha map
            if (!mat.map_d.empty()) {
                mask_id = load_texture(path.base_name() + "/" + mat.map_d);
            }
        }

        if (mask_id >= 0) {
            auto offset = mask_map.find(mask_id);
            const auto& image = scene.texture(mask_id)->image();
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

    scene.upload_mask_buffer(masks);
}

void create_mesh(const obj::File& obj_file, Scene& scene, MtlLightBuffer& mtl_to_light_intensity) {
    // This function creates a big mesh out of the whole scene.
    scene.meshes().emplace_back();

    auto& mesh = scene.mesh(0);

    // Add attributes for texture coordinates and normals
    mesh.add_attribute(Mesh::AttributeType::FLOAT2);
    mesh.add_attribute(Mesh::AttributeType::FLOAT3);
    mesh.add_attribute(Mesh::AttributeType::FLOAT3, Mesh::AttributeBinding::PER_FACE);

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

                    auto iter = mtl_to_light_intensity.find(face.material);
                    if (iter != mtl_to_light_intensity.end()) {
                        auto mat = scene.material(face.material).get();

                        auto p0 = obj_file.vertices[face.indices[0].v];
                        auto p1 = obj_file.vertices[face.indices[i].v];
                        auto p2 = obj_file.vertices[face.indices[i+1].v];

                        // Create a light source for this emissive object.
                        scene.lights().emplace_back(new TriangleLight(iter->second, p0, p1, p2));
                        mat->set_light(scene.lights().back().get());
                    }

                    prev = next;
                }
            }
        }

        if (triangles.size() == 0) continue;

        // Create a mesh for this object
        int vert_offset = mesh.vertex_count();
        int idx_offset = mesh.index_count();
        mesh.set_index_count(idx_offset + triangles.size() * 4);
        for (TriIdx t : triangles) {
            mesh.indices()[idx_offset++] = t.v0 + vert_offset;
            mesh.indices()[idx_offset++] = t.v1 + vert_offset;
            mesh.indices()[idx_offset++] = t.v2 + vert_offset;
            mesh.indices()[idx_offset++] = t.m;
        }

        mesh.set_vertex_count(vert_offset + mapping.size());

        for (auto& p : mapping) {
            const auto& v = obj_file.vertices[p.first.v];
            mesh.vertices()[vert_offset + p.second].x = v.x;
            mesh.vertices()[vert_offset + p.second].y = v.y;
            mesh.vertices()[vert_offset + p.second].z = v.z;
        }

        if (has_texcoords) {
            auto texcoords = mesh.attribute<float2>(MeshAttributes::TEXCOORDS);
            // Set up mesh texture coordinates
            for (auto& p : mapping) {
                const auto& t = obj_file.texcoords[p.first.t];
                texcoords[vert_offset + p.second] = t;
            }
        }

        if (has_normals) {
            auto normals = mesh.attribute<float3>(MeshAttributes::NORMALS);
            // Set up mesh normals
            for (auto& p : mapping) {
                const auto& n = obj_file.normals[p.first.n];
                normals[vert_offset + p.second] = n;
            }
        } else {
            // Recompute normals
            std::cout << "  Recomputing normals..." << std::flush;
            mesh.compute_normals(MeshAttributes::NORMALS);
            std::cout << std::endl;
        }
    }

    printf("triangles: %d \n", mesh.triangle_count());

    auto geom_normals = mesh.attribute<float3>(MeshAttributes::GEOM_NORMALS);
    for (int i = 0; i < mesh.triangle_count(); ++i) {
        auto t = mesh.triangle(i);
        geom_normals[i] = normalize(cross(t[1] - t[0], t[2] - t[0]));
    }
}

bool parse_scene_file(const Path& path, Scene& scene, std::string& obj_filename, float3& cam_pos, float3& cam_dir, float3& cam_up, std::string& accel_filename) {
    std::ifstream stream(path);

    std::string cmd;

    bool pos_given = false;
    bool dir_given = false;
    bool up_given = false;

    while (stream >> cmd) {
        if (cmd[0] == '#') {
            // Ignore comments
            stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        if (cmd == "pos") {
            if (!(stream >> cam_pos.x) ||
                !(stream >> cam_pos.y) ||
                !(stream >> cam_pos.z)) {
                std::cout << " Unexpected EOF in camera position" << std::endl;
                return false;
            }
            pos_given = true;
        } else if (cmd == "dir") {
            if (!(stream >> cam_dir.x) ||
                !(stream >> cam_dir.y) ||
                !(stream >> cam_dir.z)) {
                std::cout << " Unexpected EOF in camera direction" << std::endl;
                return false;
            }
            dir_given = true;
        } else if (cmd == "up") {
            if (!(stream >> cam_up.x) ||
                !(stream >> cam_up.y) ||
                !(stream >> cam_up.z)) {
                std::cout << " Unexpected EOF in camera up vector" << std::endl;
                return false;
            }
            up_given = true;
        } else if (cmd == "mesh") {
            if (obj_filename != "") {
                std::cout << " Multiple .obj files in one scene are not supported yet." << std::endl;
                return false;
            }

            // Mesh file is the entire remainder of this line (can include whitespace)
            //std::getline(stream, obj_filename);
            if (!(stream >> obj_filename)) {
                std::cout << " Error reading the obj filename" << std::endl;
                return false;
            }
        } else if (cmd == "dir_light") {
            float3 dir;
            float3 intensity;

            if (!(stream >> dir.x) ||
                !(stream >> dir.y) ||
                !(stream >> dir.z)) {
                std::cout << " Unexpected EOF in directional light direction" << std::endl;
                return false;
            }

            if (!(stream >> intensity.x) ||
                !(stream >> intensity.y) ||
                !(stream >> intensity.z)) {
                std::cout << " Unexpected EOF in directional light intensity" << std::endl;
                return false;
            }

            scene.lights().emplace_back(new DirectionalLight(normalize(dir), float4(intensity, 1.0f), scene.bounding_sphere()));
        } else if (cmd == "point_light") {
            float3 pos;
            float3 intensity;

            if (!(stream >> pos.x) ||
                !(stream >> pos.y) ||
                !(stream >> pos.z)) {
                std::cout << " Unexpected EOF in point light position" << std::endl;
                return false;
            }

            if (!(stream >> intensity.x) ||
                !(stream >> intensity.y) ||
                !(stream >> intensity.z)) {
                std::cout << " Unexpected EOF in point light intensity" << std::endl;
                return false;
            }

            scene.lights().emplace_back(new PointLight(pos, float4(intensity, 1.0f)));
        } else if (cmd == "accel") {
            if (accel_filename != "") {
                std::cout << " Multiple acceleration structure files in one scene are not supported." << std::endl;
                return false;
            }

            // The BVH filename is the entire remainder of this line (can include whitespace)
            //std::getline(stream, accel_filename);
            if (!(stream >> accel_filename)) {
                std::cout << " Error reading the acceleration structure filename" << std::endl;
                return false;
            }
        }
    }

    return obj_filename != "" && pos_given && dir_given && up_given;
}

bool build_scene(const Path& path, Scene& scene, float3& cam_pos, float3& cam_dir, float3& cam_up) {
    std::string obj_filename;
    std::string accel_filename;
    std::cout << "[1/8] Parsing Scene File..." << std::flush;
    if (!parse_scene_file(path, scene, obj_filename, cam_pos, cam_dir, cam_up, accel_filename)) {
        std::cout << " FAILED" << std::endl;
        return false;
    }
    std::cout << std::endl;

    Path obj_path(path.base_name() + '/' + obj_filename);
    obj::File obj_file;

    std::cout << "[2/8] Loading OBJ..." << std::flush;
    if (!load_obj(obj_path, obj_file)) {
        std::cout << " FAILED" << std::endl;
        return false;
    }
    std::cout << std::endl;

    obj::MaterialLib mtl_lib;

    // Parse the associated MTL files
    std::cout << "[3/8] Loading MTLs..." << std::flush;
    for (auto& lib : obj_file.mtl_libs) {
        if (!load_mtl(obj_path.base_name() + "/" + lib, mtl_lib)) {
            std::cout << " FAILED" << std::endl;
            return false;
        }
    }
    std::cout << std::endl;

    // Store the light intensities read from the materials. Used to create the actual light sources once the geometry is known.
    MtlLightBuffer mtl_to_light_intensity;

    std::cout << "[4/8] Converting materials..." << std::endl;
    convert_materials(obj_path, obj_file, mtl_lib, scene, mtl_to_light_intensity);

    // Create a scene from the OBJ file.
    std::cout << "[5/8] Creating scene..." << std::endl;
    create_mesh(obj_file, scene, mtl_to_light_intensity);

    // Check for invalid normals
    std::cout << "[6/8] Validating scene..." << std::endl;

    /*bool bad_normals = false;

    auto& mesh = scene.mesh(0);
    auto normals = mesh.attribute<float3>(MeshAttributes::NORMALS);
    for (int i = 0; i < mesh.vertex_count(); i++) {
        auto& n = normals[i];
        if (isnan(n.x) || isnan(n.y) || isnan(n.z)) {
            n.x = 0;
            n.y = 1;
            n.z = 0;
            bad_normals = true;
        }
    }

    if (bad_normals) std::cout << "  Normals containing invalid values have been replaced" << std::endl;

    if (mesh.triangle_count() == 0) {
        std::cout << " There is no triangle in the scene." << std::endl;
        return false;
    }

    if (scene.lights().empty()) {
        std::cout << "  There are no lights in the scene." << std::endl;
        return false;
    }*/

    /*scene.texcoords = std::move(ThorinArray<::Vec2>(mesh.vertex_count()));
    {
        auto texcoords = mesh.attribute<float2>(MeshAttributes::TEXCOORDS);
        for (int i = 0; i < mesh.vertex_count(); i++) {
            scene.texcoords[i].x = texcoords[i].x;
            scene.texcoords[i].y = texcoords[i].y;
        }
    }

    scene.indices = std::move(ThorinArray<int>(mesh.index_count()));
    {
        for (int i = 0; i < mesh.index_count(); i++)
            scene.indices[i] = mesh.indices()[i];
    }*/

    bool accel_loaded = false;
    /*if (accel_filename != "") {
        std::cout << "[7/8] Loading acceleration structure from file..." << std::flush;

        auto accel_path = path.base_name() + '/' + accel_filename;

        if (!load_accel(accel_path, scene.nodes, scene.tris))
            std::cout << " FAILED. Rebuilding..." << std::endl;
        else {
            accel_loaded = true;
            std::cout << std::endl;
        }
    }*/

    // If no bvh file was specified, or loading has failed, build a new one.
    //if (!accel_loaded) {
        std::cout << "[7/8] Building acceleration structure..." << std::endl;

//         std::vector<::Node> nodes;
//         std::vector<::Vec4> tris;
//         std::unique_ptr<Adapter> adapter = new_adapter(nodes, tris);
//         adapter->build_accel(scene.mesh);
// #ifdef STATISTICS
//         std::cout << "  "; adapter->print_stats();
// #endif
//         scene.nodes = nodes;
//         scene.tris = tris;
//     }

    scene.build_mesh_accels();

    // TEST

    for (auto& m : scene.meshes()) {
        m.compute_bounding_box();
    }

    for (int i = 0; i < 2; ++i) {
        float4x4 mat = float4x4::translation(1.0f * (i), 0.0f, 0.0f);
        //float4x4 mat = float4x4::scaling(2.0f, 2.0f, 2.0f);
        scene.instances().emplace_back(0, mat);
    }

    // END TEST

    scene.build_top_level_accel();

    // Compute bounding sphere.
    // BBox mesh_bb = BBox::empty();
    // for (size_t i = 0; i < scene.mesh.vertex_count(); i++) {
    //     const float3 v(scene.mesh.vertices()[i]);
    //     mesh_bb.extend(v);
    // }
    // const float radius = length(mesh_bb.max - mesh_bb.min) * 0.5f;
    // scene.sphere.inv_radius_sqr = 1.0f / sqr(radius);
    // scene.sphere.radius = radius;
    // scene.sphere.center = (mesh_bb.max + mesh_bb.min) * 0.5f;
    scene.compute_bounding_sphere();

    std::cout << "[8/8] Moving the scene to the device..." << std::flush;
    // scene.nodes.upload();
    // scene.tris.upload();
    // scene.masks.upload();
    // scene.mask_buffer.upload();
    // scene.indices.upload();
    // scene.texcoords.upload();

    scene.upload_mesh_accels();
    scene.upload_top_level_accel();

    std::cout << std::endl;

    return true;
}

} // namespace imba
