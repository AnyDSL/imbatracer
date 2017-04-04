#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include "materials/materials.h"
#include "light.h"
#include "ray_queue.h"

#include "../core/mesh.h"
#include "../core/mask.h"

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
using TextureContainer = std::vector<std::unique_ptr<TextureSampler>>;
using MaterialContainer = std::vector<std::unique_ptr<Material>>;
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

    /// Uploads the mask buffer to use for traversal.
    void upload_mask_buffer(const MaskBuffer&);
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
    CONTAINER_ACCESSORS(texture,  textures,  std::unique_ptr<TextureSampler>, TextureContainer)
    CONTAINER_ACCESSORS(material, materials, std::unique_ptr<Material>,       MaterialContainer)
    CONTAINER_ACCESSORS(mesh,     meshes,    Mesh,                            MeshContainer)
    CONTAINER_ACCESSORS(instance, instances, Mesh::Instance,                  InstanceContainer)

#undef CONTAINER_ACCESSORS

    const TraversalData& traversal_data_gpu() const { assert(gpu_buffers_); return traversal_gpu_; }
    const TraversalData& traversal_data_cpu() const { assert(cpu_buffers_); return traversal_cpu_; }

    const BSphere& bounding_sphere() const { return sphere_; }

    int local_tri_id(int tri_id, int mesh_id) const {
        return tri_id - tri_layout_[mesh_id];
    }

    void set_env_map(EnvMap* map) {
        env_map_.reset(map);
    }

    const EnvMap* env_map() const {
        return env_map_.get();
    }

private:
    bool cpu_buffers_;
    bool gpu_buffers_;

    void setup_traversal_buffers();
    void setup_traversal_buffers_cpu();
    void setup_traversal_buffers_gpu();

    LightContainer     lights_;
    TextureContainer   textures_;
    MaterialContainer  materials_;
    MeshContainer      meshes_;
    InstanceContainer  instances_;

    TraversalData traversal_gpu_;
    TraversalData traversal_cpu_;

    std::vector<traversal_gpu::Node> top_nodes_gpu_;
    std::vector<traversal_gpu::Node> nodes_gpu_;
    std::vector<Vec4> tris_gpu_;
    std::vector<int>  layout_gpu_;
    int node_count_gpu_;

    std::vector<traversal_cpu::Node> top_nodes_cpu_;
    std::vector<traversal_cpu::Node> nodes_cpu_;
    std::vector<Vec4> tris_cpu_;
    std::vector<int>  layout_cpu_;
    int node_count_cpu_;

    std::vector<Vec2> texcoord_buf_;
    std::vector<int>  index_buf_;
    std::vector<int>  tri_layout_;
    std::vector<InstanceNode> instance_nodes_;

    BSphere sphere_;

    std::unique_ptr<EnvMap> env_map_;
};

} // namespace imba

#endif // IMBA_SCENE_H
