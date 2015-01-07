#ifndef IMBA_SCENE_HPP
#define IMBA_SCENE_HPP

#include <unordered_set>
#include <memory>

#include "image.hpp"
#include "triangle_mesh.hpp"
#include "material.hpp"

namespace imba {

template <typename T>
struct SceneObjectId {
    struct Hash {
        size_t operator() (const SceneObjectId& i) const { return i.id; }
    };

    SceneObjectId();
    SceneObjectId(int i) : id(i) {}

    bool operator == (const SceneObjectId& i) const { return id == i.id; }

    int id;
};

typedef SceneObjectId<TriangleMesh> TriangleMeshId;
typedef SceneObjectId<Texture>      TextureId;
typedef SceneObjectId<Material>     MaterialId;

/// Scene represented as a collection of renderable objects, which can be
/// triangle mesh instances, CSG primitives, and so on.
class Scene {
    friend class Render;
public:
    Scene() : dirty_(true) {}

    ~Scene();

    TriangleMeshId add_triangle_mesh(TriangleMesh* mesh) {
        meshes_.push_back(mesh);

        TriangleMeshId id(meshes_.size() - 1);
        sync_.dirty_meshes.insert(id);
        dirty_ = true;
        return id;
    }

    TextureId add_texture(Texture* texture) {
        textures_.push_back(texture);

        TextureId id(textures_.size() - 1);
        sync_.dirty_textures.insert(id);
        dirty_ = true;
        return id;
    }

    int triangle_mesh_count() const { return meshes_.size(); }
    int texture_count() const { return textures_.size(); }

    const TriangleMesh* triangle_mesh(TriangleMeshId id) const { return meshes_[id.id]; }
    TriangleMesh* triangle_mesh(TriangleMeshId id) {
        sync_.dirty_meshes.insert(id);
        dirty_ = true;
        return meshes_[id.id];
    }

    const Texture* texture(TextureId id) const { return textures_[id.id]; }
    Texture* texture(TextureId id) {
        sync_.dirty_textures.insert(id);
        dirty_ = true;
        return textures_[id.id];
    }

private:
    void synchronize();

    std::vector<TriangleMesh*> meshes_;
    std::vector<Texture*>      textures_;
    std::vector<Material>      materials_;

    struct {
        ThorinUniquePtr<::Scene>         scene_data;
        ThorinUniquePtr<::CompiledScene> comp_scene;

        ThorinVector<::Texture>      textures;
        ThorinVector<::Mesh>         meshes;

        std::unordered_set<TriangleMeshId, TriangleMeshId::Hash> dirty_meshes;
        std::unordered_set<TextureId, TextureId::Hash> dirty_textures;
    } sync_;

    bool dirty_;
};

} // namespace imba

#endif // IMBA_SCENE_HPP

