#ifndef IMBA_SCENE_HPP
#define IMBA_SCENE_HPP

#include <unordered_set>
#include <algorithm>
#include <memory>

#include "object.hpp"
#include "image.hpp"
#include "triangle_mesh.hpp"
#include "instance.hpp"
#include "material.hpp"
#include "proxy.hpp"
#include "../common/matrix.hpp"

namespace imba {

/// Scene represented as a collection of renderable objects, which can be
/// triangle mesh instances, CSG primitives, and so on.
class Scene {
    friend class Render;

    friend class SceneAccess<TriangleMesh>;
    friend class SceneAccess<Texture>;
    friend class SceneAccess<Instance>;

public:
    Scene();
    ~Scene();

    template <typename... Args>
    TriangleMeshId new_triangle_mesh(Args... args) {
        meshes_.push_back(new TriangleMesh(args...));
        TriangleMeshId id(meshes_.size() - 1);
        sync_.dirty_meshes.insert(id.id);
        dirty_ = true;
        return id;
    }

    template <typename... Args>
    TextureId new_texture(Args... args) {
        textures_.push_back(new Texture(args...));
        TextureId id(textures_.size() - 1);
        sync_.dirty_textures.insert(id.id);
        dirty_ = true;
        return id;
    }

    template <typename... Args>
    InstanceId new_instance(Args... args) {
        Instance inst(args...);
        sync_.instances.push_back(*reinterpret_cast<::MeshInstance*>(&inst));
        dirty_ = true;
        return InstanceId(sync_.instances.size() - 1);
    }

    int triangle_mesh_count() const { return meshes_.size(); }
    int instance_count() const { return sync_.instances.size(); }
    int texture_count() const { return textures_.size(); }

    ReadOnlyProxy<TriangleMesh> triangle_mesh(TriangleMeshId id) const {
        return ReadOnlyProxy<TriangleMesh>(this, id.id);
    }

    ReadWriteProxy<TriangleMesh> triangle_mesh(TriangleMeshId id) {
        return ReadWriteProxy<TriangleMesh>(this, id.id);
    }

    ReadOnlyProxy<Texture> texture(TextureId id) const {
        return ReadOnlyProxy<Texture>(this, id.id);
    }

    ReadWriteProxy<Texture> texture(TextureId id) {
        return ReadWriteProxy<Texture>(this, id.id);
    }

    ReadOnlyProxy<Instance> instance(InstanceId id) const {
        return ReadOnlyProxy<Instance>(this, id.id);
    }

    ReadWriteProxy<Instance> instance(InstanceId id) {
        return ReadWriteProxy<Instance>(this, id.id);
    }

    /// Forces compilation of the scene. Makes sure everything is ready for rendering.
    void compile() const;

private:
    std::vector<TriangleMesh*> meshes_;
    std::vector<Texture*>      textures_;
    std::vector<Material>      materials_;

    mutable struct {
        ThorinUniquePtr<::Scene>         scene_data;
        ThorinUniquePtr<::CompiledScene> comp_scene;

        ThorinVector<::Texture>      textures;
        ThorinVector<::Mesh>         meshes;
        ThorinVector<::MeshInstance> instances;

        ThorinVector<int> to_refit;
        ThorinVector<int> to_rebuild;

        std::unordered_set<int> dirty_meshes;
        std::unordered_set<int> dirty_textures;
    } sync_;

    mutable bool dirty_;
};

template <>
struct SceneAccess<TriangleMesh> {
    static const TriangleMesh* read_only(const Scene* scene, int id) {
        return scene->meshes_[id];
    }

    static TriangleMesh* read_write(Scene* scene, int id) {
        return scene->meshes_[id];
    }

    static void notify_change(Scene* scene, int id) {
        scene->dirty_ = true;
        scene->sync_.dirty_meshes.insert(id);
    }
};

template <>
struct SceneAccess<Texture> {
    static const Texture* read_only(const Scene* scene, int id) {
        return scene->textures_[id];
    }

    static Texture* read_write(Scene* scene, int id) {
        return scene->textures_[id];
    }

    static void notify_change(Scene* scene, int id) {
        scene->dirty_ = true;
        scene->sync_.dirty_textures.insert(id);
    }
};

template <>
struct SceneAccess<Instance> {
    static const Instance* read_only(const Scene* scene, int id) {
        return reinterpret_cast<const Instance*>(&scene->sync_.instances[0] + id);
    }

    static Instance* read_write(Scene* scene, int id) {
        return reinterpret_cast<Instance*>(&scene->sync_.instances[0] + id);
    }

    static void notify_change(Scene* scene, int id) {}
};

} // namespace imba

#endif // IMBA_SCENE_HPP

