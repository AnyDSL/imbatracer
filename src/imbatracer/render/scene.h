#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include "imbatracer/render/materials/material_system.h"
#include "imbatracer/render/light.h"
#include "imbatracer/render/scheduling/ray_queue.h"

#include "imbatracer/core/mesh.h"
#include "imbatracer/core/mask.h"

namespace imba {

/// Mesh attributes used by the scene.
struct MeshAttributes {
    enum {
        TEXCOORDS = 0,
        NORMALS = 1,
        GEOM_NORMALS = 2
    };
};

using LightContainer = std::vector<std::unique_ptr<Light>>;
//using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;
// using MaterialContainer = std::vector<std::unique_ptr<Material>>;
using MeshContainer = std::vector<Mesh>;
using InstanceContainer = std::vector<Mesh::Instance>;

/// Stores all data required to render a scene.
class Scene {
public:
    Scene(bool cpu_buffers = false, bool gpu_buffers = true)
        : cpu_buffers_(cpu_buffers)
        , gpu_buffers_(gpu_buffers)
    {
        if (!cpu_buffers && !gpu_buffers) {
            std::cout << "Neither CPU nor GPU traversal was enabled!" << std::endl;
            exit(0);
        }
    }

    /// Builds an acceleration structure for every mesh in the scene.
    void build_mesh_accels(const std::vector<std::string>& accel_filenames);
    /// Builds a top-level acceleration structure.
    /// All the mesh acceleration structures must have been built before this call.
    void build_top_level_accel();

    /// Uploads all mesh acceleration structures on the device.
    void upload_mesh_accels();
    /// Uploads the top-level acceleration structure on the device.
    /// The top-level acceleration structure must have been built before this call.
    void upload_top_level_accel();

    /// Computes the bounding sphere of the scene.
    void compute_bounding_sphere();

#define CONTAINER_ACCESSORS(name, names, Type, ContainerType) \
    const Type& name(int i) const { return names##_[i]; } \
    Type& name(int i) { return names##_[i]; } \
    const ContainerType& names() const { return names##_; } \
    ContainerType& names() { return names##_; } \
    size_t name##_count() const { return names##_.size(); }

    CONTAINER_ACCESSORS(light,    lights,    std::unique_ptr<Light>,          LightContainer)
    //CONTAINER_ACCESSORS(texture,  textures,  std::unique_ptr<TextureSampler>, TextureContainer)
    // CONTAINER_ACCESSORS(material, materials, std::unique_ptr<Material>,       MaterialContainer)
    CONTAINER_ACCESSORS(mesh,     meshes,    Mesh,                            MeshContainer)
    CONTAINER_ACCESSORS(instance, instances, Mesh::Instance,                  InstanceContainer)

#undef CONTAINER_ACCESSORS

    const TraversalData<traversal_gpu::Node>& traversal_data_gpu() const { assert(gpu_buffers_); return traversal_gpu_; }
    const TraversalData<traversal_cpu::Node>& traversal_data_cpu() const { assert(cpu_buffers_); return traversal_cpu_; }

    bool has_gpu_buffers() const { return gpu_buffers_; }
    bool has_cpu_buffers() const { return cpu_buffers_; }

    const BSphere& bounding_sphere() const { return sphere_; }
    const BBox& bounds() const { return scene_bb_; }

    int local_tri_id(int tri_id, int mesh_id) const {
        return tri_id - tri_layout_[mesh_id];
    }

    /// Determines the material id of the triangle hit
    int mat_id(const Hit& hit) const {
        const auto& inst = instance(hit.inst_id);
        const auto& m = mesh(inst.id);
        const int tri = local_tri_id(hit.tri_id, inst.id);
        const int mat = m.indices()[tri * 4 + 3];
        return mat;
    }

    void set_env_map(EnvMap* map) {
        env_map_.reset(map);
    }

    const EnvMap* env_map() const {
        return env_map_.get();
    }

    /// Creates a new material system.
    /// \param path the search path for .oso files (OpenShadingLanguage compiled shader files)
    void create_mat_sys(const std::string& path) {
        mat_sys_.reset(new MaterialSystem(path));
    }

    MaterialSystem* material_system() { return mat_sys_.get(); }
    const MaterialSystem* material_system() const { return mat_sys_.get(); }

    /// Adds the OSL material with the given name to the scene.
    /// \returns the id of the newly added material.
    int add_material(const std::string& search_path, const std::string& name, const std::string& serialized_graph) {
        mat_sys_->add_shader(search_path, name, serialized_graph);
        return mat_sys_->shader_count() - 1;
    }

    int material_count() const {
        return mat_sys_->shader_count();
    }

    Intersection calculate_intersection(const Hit& hit, const Ray& ray) const {
        const Mesh::Instance& inst = instance(hit.inst_id);
        const Mesh& m = mesh(inst.id);

        const int l_tri = local_tri_id(hit.tri_id, inst.id);

        const int i0  = m.indices()[l_tri * 4 + 0];
        const int i1  = m.indices()[l_tri * 4 + 1];
        const int i2  = m.indices()[l_tri * 4 + 2];
        const int mat = m.indices()[l_tri * 4 + 3];

        const float3     org(ray.org.x, ray.org.y, ray.org.z);
        const float3 out_dir(ray.dir.x, ray.dir.y, ray.dir.z);
        const auto       pos = org + hit.tmax * out_dir;
        const auto local_pos = inst.inv_mat * float4(pos, 1.0f);

        // Recompute v based on u and local_pos
        const float u = hit.u;
        const auto v0 = float3(m.vertices()[i0]);
        const auto e1 = float3(m.vertices()[i1]) - v0;
        const auto e2 = float3(m.vertices()[i2]) - v0;
        const float v = dot(local_pos - v0 - u * e1, e2) / dot(e2, e2);

        const auto texcoords    = m.attribute<float2>(MeshAttributes::TEXCOORDS);
        const auto normals      = m.attribute<float3>(MeshAttributes::NORMALS);
        const auto geom_normals = m.attribute<float3>(MeshAttributes::GEOM_NORMALS);

        const auto uv_coords    = lerp(texcoords[i0], texcoords[i1], texcoords[i2], u, v);
        const auto local_normal = lerp(normals[i0], normals[i1], normals[i2], u, v);
        const auto normal       = normalize(float3(local_normal * inst.inv_mat));
        const auto geom_normal  = normalize(float3(geom_normals[l_tri] * inst.inv_mat));

        const auto w_out = -normalize(out_dir);

        float area = length(cross(e1, e2)) * 0.5f * inst.det; // TODO precompute this?

        Intersection res {
            pos, w_out, normal, uv_coords, geom_normal, area, mat, hit.tmax * hit.tmax
        };

        // Ensure that the shading normal is always in the same hemisphere as the geometric normal.
        if (dot(res.geom_normal, res.normal) < 0.0f)
            res.normal = -res.normal;

        return res;
    }

private:
    template <typename Node>
    struct BuildAccelData {
        std::vector<Node> top_nodes;
        std::vector<Node> nodes;
        std::vector<Vec4> tris;
        std::vector<int>  layout;
        int node_count;
    };

    bool cpu_buffers_;
    bool gpu_buffers_;

    template <typename Node>
    void setup_traversal_buffers(BuildAccelData<Node>&, TraversalData<Node>&, anydsl::Platform);
    template <typename Node, typename NewAdapterFn>
    void build_top_level_accel(BuildAccelData<Node>&, NewAdapterFn);
    template <typename Node, typename NewAdapterFn, typename LoadAccelFn, typename StoreAccelFn>
    void build_mesh_accels(BuildAccelData<Node>&, const std::vector<std::string>&, NewAdapterFn, LoadAccelFn, StoreAccelFn);
    template <typename Node>
    void upload_mesh_accels(BuildAccelData<Node>&, TraversalData<Node>&);
    template <typename Node>
    void upload_top_level_accel(BuildAccelData<Node>&, TraversalData<Node>&);

    void setup_traversal_buffers();

    LightContainer     lights_;
    // TextureContainer   textures_;
    // MaterialContainer  materials_;
    MeshContainer      meshes_;
    InstanceContainer  instances_;

    TraversalData<traversal_gpu::Node> traversal_gpu_;
    TraversalData<traversal_cpu::Node> traversal_cpu_;

    BuildAccelData<traversal_gpu::Node> build_gpu_;
    BuildAccelData<traversal_cpu::Node> build_cpu_;

    std::vector<Vec2> texcoord_buf_;
    std::vector<int>  index_buf_;
    std::vector<int>  tri_layout_;
    std::vector<InstanceNode> instance_nodes_;

    BSphere sphere_;
    BBox scene_bb_;

    std::unique_ptr<EnvMap> env_map_;
    std::unique_ptr<MaterialSystem> mat_sys_;
};

} // namespace imba

#endif // IMBA_SCENE_H
