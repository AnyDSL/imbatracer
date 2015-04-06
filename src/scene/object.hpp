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
    SceneObjectId() : id(-1) {}
    SceneObjectId(int i) : id(i) {}
    operator bool () const { return id >= 0; }
    int id;
};

typedef SceneObjectId<SceneObject::TriangleMesh> TriangleMeshId;
typedef SceneObjectId<SceneObject::Texture>      TextureId;
typedef SceneObjectId<SceneObject::Instance>     InstanceId;
typedef SceneObjectId<SceneObject::Material>     MaterialId;
typedef SceneObjectId<SceneObject::Light>        LightId;

} // namespace imba

#endif // IMBA_OBJECT_HPP

