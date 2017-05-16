#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include "imbatracer/render/materials/materials.h"
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

    const TraversalData<traversal_gpu::Node>& traversal_data_gpu() const { assert(gpu_buffers_); return traversal_gpu_; }
    const TraversalData<traversal_cpu::Node>& traversal_data_cpu() const { assert(cpu_buffers_); return traversal_cpu_; }

    bool has_gpu_buffers() const { return gpu_buffers_; }
    bool has_cpu_buffers() const { return cpu_buffers_; }

    const BSphere& bounding_sphere() const { return sphere_; }
    const BBox& bounds() const { return scene_bb_; }

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
    void upload_mask_buffer(TraversalData<Node>&, anydsl::Platform, const MaskBuffer&);
    template <typename Node>
    void upload_mesh_accels(BuildAccelData<Node>&, TraversalData<Node>&);
    template <typename Node>
    void upload_top_level_accel(BuildAccelData<Node>&, TraversalData<Node>&);

    void setup_traversal_buffers();

    LightContainer     lights_;
    TextureContainer   textures_;
    MaterialContainer  materials_;
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
};

} // namespace imba

#endif // IMBA_SCENE_H
