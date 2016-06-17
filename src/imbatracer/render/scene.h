#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include <traversal.h>

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
    Scene() {}

    /// Builds an acceleration structure for every mesh in the scene.
    void build_mesh_accels();
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

    const TraversalData& traversal_data() const { return traversal_; }

    const BSphere& bounding_sphere() const { return sphere_; }

    int local_tri_id(int tri_id, int mesh_id) const {
        return tri_id - tri_layout_[mesh_id];
    }

private:
    void setup_traversal_buffers();

    LightContainer     lights_;
    TextureContainer   textures_;
    MaterialContainer  materials_;
    MeshContainer      meshes_;
    InstanceContainer  instances_;

    TraversalData traversal_;

    std::vector<Node> top_nodes_;
    std::vector<Node> nodes_;
    std::vector<Vec4> tris_;
    std::vector<int>  layout_;
    std::vector<Vec2> texcoord_buf_;
    std::vector<int>  index_buf_;
    std::vector<int>  tri_layout_;
    std::vector<InstanceNode> instance_nodes_;
    int node_count_;

    BSphere sphere_;
};

} // namespace imba

#endif // IMBA_SCENE_H
