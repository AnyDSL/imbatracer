#ifndef IMBA_SCENE_HPP
#define IMBA_SCENE_HPP

#include <unordered_set>
#include <memory>
#include "image_buffer.hpp"
#include "triangle_mesh.hpp"
#include "material.hpp"

namespace imba {

/// Scene represented as a collection of renderable objects, which can be
/// triangle mesh instances, CSG primitives, and so on.
class Scene {
private:
    template <typename T>
    struct SceneObjectId {
        SceneObjectId(int i) : id_(i) {}
        int id_;
    };

public:
    typedef SceneObjectId<TriangleMesh> TriangleMeshId;
    typedef SceneObjectId<ImageBuffer>  TextureId;
    typedef SceneObjectId<Material>     MaterialId;

    ~Scene() {
        for (auto mesh : meshes_) { delete mesh; }
        for (auto tex : textures_) { delete tex; }
    }

    TriangleMeshId add_triangle_mesh(TriangleMesh* mesh) {
        meshes_.push_back(mesh);
        return TriangleMeshId(meshes_.size() - 1);
    }

    TextureId add_texture(ImageBuffer* texture) {
        textures_.push_back(texture);
        return TextureId(textures_.size() - 1);
    }

private:
    std::vector<TriangleMesh*> meshes_;
    std::vector<ImageBuffer*>  textures_;
    std::vector<Material>      materials_;
};

} // namespace imba

#endif // IMBA_SCENE_HPP

