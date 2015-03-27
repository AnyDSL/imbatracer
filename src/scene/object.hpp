#ifndef IMBA_OBJECT_HPP
#define IMBA_OBJECT_HPP

namespace imba {

enum class SceneObject {
    TriangleMesh,
    Texture,
    Instance,
    Material,
    Light
};

template <SceneObject Object>
struct SceneObjectId {
    SceneObjectId() {}
    SceneObjectId(int i) : id(i) {}
    int id;
};

typedef SceneObjectId<SceneObject::TriangleMesh> TriangleMeshId;
typedef SceneObjectId<SceneObject::Texture>      TextureId;
typedef SceneObjectId<SceneObject::Instance>     InstanceId;
typedef SceneObjectId<SceneObject::Material>     MaterialId;
typedef SceneObjectId<SceneObject::Light>        LightId;

} // namespace imba

#endif // IMBA_OBJECT_HPP

