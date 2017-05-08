#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <memory>
#include <fstream>

// For isnan
#include <math.h>

#include "build_scene.h"
#include "loaders/loaders.h"
#include "core/adapter.h"

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

using MtlLightBuffer = std::unordered_map<int, rgb>;

void convert_materials(const Path& path, const obj::File& obj_file, const obj::MaterialLib& mtl_lib, Scene& scene,
                       MtlLightBuffer& mtl_to_light_intensity, MaskBuffer& masks) {
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
                mtl_to_light_intensity.insert(std::make_pair(scene.material_count(), mat.ke));

            TextureSampler* bump_sampler = nullptr;
            if (!mat.map_bump.empty()) {
                // Load the bump map.
                const std::string img_file = path.base_name() + "/" + mat.map_bump;
                int sampler_id = load_texture(img_file);
                if (sampler_id >= 0)
                    bump_sampler = scene.texture(sampler_id).get();
            }

            if (mat.illum == 5)
                scene.materials().emplace_back(new MirrorMaterial(1.0f, mat.ns, mat.ks, bump_sampler));
            else if (mat.illum == 7)
                scene.materials().emplace_back(new GlassMaterial(mat.ni, mat.tf, mat.ks, bump_sampler));
            else if (is_phong){
                Material* mtl;
                if (!mat.map_kd.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_kd;

                    int sampler_id = load_texture(img_file);
                    if (sampler_id < 0) {
                        mtl = new GlossyMaterial(mat.ns, mat.ks, rgb(1.0f, 0.0f, 1.0f), bump_sampler);
                    } else {
                        mtl = new GlossyMaterial(mat.ns, mat.ks, scene.texture(sampler_id).get(), bump_sampler);
                    }
                } else {
                    mtl = new GlossyMaterial(mat.ns, mat.ks, mat.kd, bump_sampler);
                }

                scene.materials().emplace_back(mtl);
            } else {
                Material* mtl;
                if (!mat.map_kd.empty()) {
                    const std::string img_file = path.base_name() + "/" + mat.map_kd;

                    int sampler_id = load_texture(img_file);
                    if (sampler_id < 0) {
                        mtl = new DiffuseMaterial(rgb(1.0f, 0.0f, 1.0f), bump_sampler);
                    } else {
                        mtl = new DiffuseMaterial(scene.texture(sampler_id).get(), bump_sampler);
                    }
                } else {
                    mtl = new DiffuseMaterial(mat.kd, bump_sampler);
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
}

void create_mesh(const obj::File& obj_file, Scene& scene, std::vector<TriangleLight>& tri_lights, MtlLightBuffer& mtl_to_light_intensity,
                 int mtl_offset, MaskBuffer& masks) {
    // This function creates a big mesh out of the whole scene.
    scene.meshes().emplace_back();

    auto& mesh = scene.meshes().back();

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
                    int mtl_idx = face.material + mtl_offset;

                    // If this is a light, we need a separate material for every face
                    // as the emitter might be different (different area)
                    auto iter = mtl_to_light_intensity.find(mtl_idx);
                    if (iter != mtl_to_light_intensity.end()) {
                        auto mat = scene.material(mtl_idx).get();
                        scene.materials().emplace_back(mat->duplicate());
                        mat = scene.materials().back().get();
                        mtl_idx = scene.materials().size() - 1;

                        // We created a new material, thus we have to add a corresponding alpha mask as well.
                        // TODO: Change this if support for masked light sources is desired.
                        masks.add_desc();

                        auto p0 = obj_file.vertices[face.indices[0].v];
                        auto p1 = obj_file.vertices[face.indices[i].v];
                        auto p2 = obj_file.vertices[face.indices[i+1].v];

                        // Create a light source for this emissive object.
                        tri_lights.emplace_back(iter->second, p0, p1, p2);
                        mat->set_emitter(new AreaEmitter(iter->second, Tri(p0, p1, p2).area()));
                    }

                    // Now emplace the triangle with either the original or the new material
                    triangles.emplace_back(v0, prev, next, mtl_idx);

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

    auto geom_normals = mesh.attribute<float3>(MeshAttributes::GEOM_NORMALS);
    for (int i = 0; i < mesh.triangle_count(); ++i) {
        auto t = mesh.triangle(i);
        geom_normals[i] = normalize(cross(t[1] - t[0], t[2] - t[0]));
    }
}

struct SceneInfo {
    std::vector<std::string> mesh_filenames;
    std::vector<std::string> accel_filenames;
    float3 cam_pos;
    float3 cam_dir;
    float3 cam_up;
};

/// Loads the scene from a file.
/// Light sources and instances are written to the scene file directly.
/// Everything else is stored in the SceneInfo structure.
bool parse_scene_file(const Path& path, Scene& scene, SceneInfo& info) {
    std::ifstream stream(path);

    std::string cmd;

    bool pos_given = false;
    bool dir_given = false;
    bool up_given = false;
    bool skip = false;

    while (skip || stream >> cmd) {
        skip = false;
        if (cmd[0] == '#') {
            // Ignore comments
            stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        if (cmd == "pos") {
            if (!(stream >> info.cam_pos.x) ||
                !(stream >> info.cam_pos.y) ||
                !(stream >> info.cam_pos.z)) {
                std::cout << " Unexpected EOF in camera position." << std::endl;
                return false;
            }
            pos_given = true;
        } else if (cmd == "dir") {
            if (!(stream >> info.cam_dir.x) ||
                !(stream >> info.cam_dir.y) ||
                !(stream >> info.cam_dir.z)) {
                std::cout << " Unexpected EOF in camera direction." << std::endl;
                return false;
            }
            dir_given = true;
        } else if (cmd == "up") {
            if (!(stream >> info.cam_up.x) ||
                !(stream >> info.cam_up.y) ||
                !(stream >> info.cam_up.z)) {
                std::cout << " Unexpected EOF in camera up vector." << std::endl;
                return false;
            }
            up_given = true;
        } else if (cmd == "mesh") {
            info.mesh_filenames.emplace_back();
            info.accel_filenames.emplace_back();

            // Mesh file is the entire remainder of this line (can include whitespace)
            if (!(stream >> info.mesh_filenames.back())) {
                std::cout << " Error reading the obj filename." << std::endl;
                return false;
            }

            // Mesh file paths are relative to the scene file.
            info.mesh_filenames.back() = path.base_name() + '/' + info.mesh_filenames.back();
        } else if (cmd == "accel") {
            if (info.accel_filenames.size() == 0) {
                std::cout << " BVH files have to be specified after the mesh they belong to." << std::endl;
                return false;
            }

            if (!(stream >> info.accel_filenames.back())) {
                std::cout << " Error reading the obj filename." << std::endl;
                return false;
            }

            // Mesh file paths are relative to the scene file.
            info.accel_filenames.back() = path.base_name() + '/' + info.accel_filenames.back();
        } else if (cmd == "dir_light") {
            float3 dir;
            float3 intensity;

            if (!(stream >> dir.x) ||
                !(stream >> dir.y) ||
                !(stream >> dir.z)) {
                std::cout << " Unexpected EOF in directional light direction." << std::endl;
                return false;
            }

            if (!(stream >> intensity.x) ||
                !(stream >> intensity.y) ||
                !(stream >> intensity.z)) {
                std::cout << " Unexpected EOF in directional light intensity." << std::endl;
                return false;
            }

            scene.lights().emplace_back(new DirectionalLight(normalize(dir), intensity, scene.bounding_sphere()));
        } else if (cmd == "point_light") {
            float3 pos;
            float3 intensity;

            if (!(stream >> pos.x) ||
                !(stream >> pos.y) ||
                !(stream >> pos.z)) {
                std::cout << " Unexpected EOF in point light position." << std::endl;
                return false;
            }

            if (!(stream >> intensity.x) ||
                !(stream >> intensity.y) ||
                !(stream >> intensity.z)) {
                std::cout << " Unexpected EOF in point light intensity." << std::endl;
                return false;
            }

            scene.lights().emplace_back(new PointLight(pos, intensity));
        } else if (cmd == "spot_light") {
            float3 pos;
            float3 dir;
            float3 intensity;
            float angle;

            if (!(stream >> pos.x) ||
                !(stream >> pos.y) ||
                !(stream >> pos.z)) {
                std::cout << " Unexpected EOF in spot light position." << std::endl;
                return false;
            }

            if (!(stream >> dir.x) ||
                !(stream >> dir.y) ||
                !(stream >> dir.z)) {
                std::cout << " Unexpected EOF in spot light direction." << std::endl;
                return false;
            }

            if (!(stream >> angle)) {
                std::cout << " Unexpected EOF in spot light angle." << std::endl;
                return false;
            }

            if (!(stream >> intensity.x) ||
                !(stream >> intensity.y) ||
                !(stream >> intensity.z)) {
                std::cout << " Unexpected EOF in spot light intensity." << std::endl;
                return false;
            }

            scene.lights().emplace_back(new SpotLight(pos, normalize(dir), radians(angle), intensity));
        } else if (cmd == "instance") {
            // Read the mesh index.
            int idx;
            if (!(stream >> idx)) {
                std::cout << " Unexpected EOF in instance." << std::endl;
                return false;
            }

            // The next lines specify the position, scale and rotation.
            int flags = 0;
            float3 pos(0.0f);
            float3 scale(1.0f);
            float3 euler(0.0f);
            while (flags != 7 && stream >> cmd) {
                if (cmd == "pos" && !(flags & 1)) {
                    if (!(stream >> pos.x) ||
                        !(stream >> pos.y) ||
                        !(stream >> pos.z)) {
                        std::cout << " Unexpected EOF in instance position." << std::endl;
                        return false;
                    }
                    flags |= 1;
                } else if (cmd == "scale" && !(flags & 2)) {
                    if (!(stream >> scale.x) ||
                        !(stream >> scale.y) ||
                        !(stream >> scale.z)) {
                        std::cout << " Unexpected EOF in instance scaling." << std::endl;
                        return false;
                    }
                    flags |= 2;
                } else if (cmd == "rot" && !(flags & 4)) {
                    if (!(stream >> euler.x) ||
                        !(stream >> euler.y) ||
                        !(stream >> euler.z)) {
                        std::cout << " Unexpected EOF in instance rotation." << std::endl;
                        return false;
                    }

                    // Convert degree to radians
                    euler.x = radians(euler.x);
                    euler.y = radians(euler.y);
                    euler.z = radians(euler.z);

                    flags |= 4;
                } else
                    // Stop at the first unknown or duplicated command. Not all attributes have to be specified.
                    break;
            }

            float4x4 mat = translate(pos.x, pos.y, pos.z) * ::imba::euler(euler.x, euler.y, euler.z) * ::imba::scale(scale.x, scale.y, scale.z);

            scene.instances().emplace_back(idx, mat);
            skip = true;
        } else if (cmd == "env") {
            if (scene.env_map() != nullptr) {
                std::cout << " Found more than one environment map. Ignoring..." << std::endl;
                continue;
            }

            std::string filename;
            float intensity;
            if (!(stream >> filename) || !(stream >> intensity)) {
                std::cout << " Unexpected EOF in the environment map parameters." << std::endl;
                return false;
            }

            filename = path.base_name() + '/' + filename;

            Image img;
            if (!load_hdr(filename, img)) {
                std::cout << " Failed to load the environment map." << std::endl;
                return false;
            }

            scene.set_env_map(new EnvMap(img, intensity, scene.bounding_sphere()));
            scene.lights().emplace_back(new EnvLight(scene.env_map(), scene.bounding_sphere()));
        }
    }

    // Validate the scene attributes.
    if (!pos_given || !dir_given || !up_given) {
        std::cout << " Camera settings not specified." << std::endl;
        return false;
    } else if (info.mesh_filenames.size() == 0) {
        std::cout << " No meshes specified." << std::endl;
        return false;
    }

    if (scene.instances().size() == 0) {
        // No instances were specified. Add an identity instance for every mesh.
        for (int i = 0; i < info.mesh_filenames.size(); ++i)
            scene.instances().emplace_back(i, float4x4::identity());
    }

    return true;
}

bool build_scene(const Path& path, Scene& scene, float3& cam_pos, float3& cam_dir, float3& cam_up) {
    SceneInfo scene_info;
    std::cout << "[1/5] Parsing Scene File..." << std::endl;
    if (!parse_scene_file(path, scene, scene_info)) {
        std::cout << " FAILED" << std::endl;
        return false;
    }
    cam_pos = scene_info.cam_pos;
    cam_dir = scene_info.cam_dir;
    cam_up  = scene_info.cam_up;
    std::cout << std::endl;

    std::cout << "[2/5] Loading mesh files..." << std::endl;
    std::vector<std::vector<TriangleLight> > tri_lights;
    MaskBuffer masks;
    for (int i = 0; i < scene_info.mesh_filenames.size(); ++i) {
        std::cout << " Mesh " << i + 1 << " of " << scene_info.mesh_filenames.size() << "..." << std::endl;

        Path obj_path(scene_info.mesh_filenames[i]);
        obj::File obj_file;
        if (!load_obj(obj_path, obj_file)) {
            std::cout << " FAILED loading obj" << std::endl;
            return false;
        }

        obj::MaterialLib mtl_lib;

        // Parse the associated MTL files
        for (auto& lib : obj_file.mtl_libs) {
            if (!load_mtl(obj_path.base_name() + "/" + lib, mtl_lib)) {
                std::cout << " FAILED loading materials" << std::endl;
                return false;
            }
        }

        MtlLightBuffer mtl_to_light_intensity;
        int mtl_offset = scene.materials().size();
        convert_materials(obj_path, obj_file, mtl_lib, scene, mtl_to_light_intensity, masks);

        tri_lights.emplace_back();
        create_mesh(obj_file, scene, tri_lights.back(), mtl_to_light_intensity, mtl_offset, masks);

        std::cout << "  validating..." << std::flush;

        bool bad_normals = false;

        auto& mesh = scene.meshes().back();
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
            std::cout << " There is no triangle in the mesh." << std::endl;
            return false;
        }

        std::cout << " done." << std::endl;
    }

    std::cout << "[3/5] Instancing light sources..." << std::endl;

    for (auto& inst : scene.instances()) {
        // Copy the triangle lights if there are any.
        for (auto& light : tri_lights[inst.id]) {
            auto p0 = inst.mat * float4(light.vertex(0), 1.0f);
            auto p1 = inst.mat * float4(light.vertex(1), 1.0f);
            auto p2 = inst.mat * float4(light.vertex(2), 1.0f);
            scene.lights().emplace_back(new TriangleLight(light.emitter()->intensity, p0, p1, p2));
        }
    }

    if (scene.lights().empty()) {
        std::cout << "  ERROR: There are no lights in the scene." << std::endl;
        return false;
    }

    std::cout << "[4/5] Building acceleration structure..." << std::endl;

    for (auto& m : scene.meshes()) {
        m.compute_bounding_box();
    }

    scene.build_mesh_accels(scene_info.accel_filenames);
    scene.build_top_level_accel();
    scene.compute_bounding_sphere();

    std::cout << "[5/5] Moving the scene to the device..." << std::flush;
    scene.upload_mesh_accels();
    scene.upload_top_level_accel();
    scene.upload_mask_buffer(masks);

    std::cout << std::endl;

    return true;
}

} // namespace imba
