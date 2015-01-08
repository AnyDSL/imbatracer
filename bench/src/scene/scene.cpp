#include <cassert>
#include "scene.hpp"
#include "../impala/impala_interface.h"

namespace imba {

Scene::Scene()
    : dirty_(true)
{
    sync_.scene_data = thorin_make_unique<::Scene>();
    sync_.scene_data->meshes       = nullptr;
    sync_.scene_data->num_meshes   = 0;
    sync_.scene_data->textures     = nullptr;
    sync_.scene_data->num_textures = 0;
    //sync_.scene_data->instances     = nullptr;
    //sync_.scene_data->num_instances = 0;
}

Scene::~Scene() {
    for (auto mesh : meshes_) { delete mesh; }
    for (auto tex : textures_) { delete tex; }

    if (sync_.comp_scene)
        ::destroy_scene(sync_.scene_data.get(), sync_.comp_scene.get());
}

void Scene::compile() {
    if (!dirty_) return;

    // Synchronize meshes
    for (auto id : sync_.dirty_meshes) {
        if (sync_.meshes.size() <= id.id) sync_.meshes.resize(id.id + 1);
        
        sync_.meshes[id.id].vertices  = reinterpret_cast<::Vec3*>(meshes_[id.id]->vertices());
        sync_.meshes[id.id].normals   = reinterpret_cast<::Vec3*>(meshes_[id.id]->vertices());
        sync_.meshes[id.id].texcoords = reinterpret_cast<::Vec2*>(meshes_[id.id]->vertices());
        sync_.meshes[id.id].materials = reinterpret_cast<int*>(meshes_[id.id]->materials());
        sync_.meshes[id.id].indices   = reinterpret_cast<int*>(meshes_[id.id]->triangles());
        sync_.meshes[id.id].num_tris  = meshes_[id.id]->triangle_count();
    }
    sync_.dirty_meshes.clear();

    // Synchronize textures
    for (auto id : sync_.dirty_textures) {
        if (sync_.textures.size() <= id.id) sync_.textures.resize(id.id + 1);
        
        sync_.textures[id.id].width  = textures_[id.id]->width();
        sync_.textures[id.id].height = textures_[id.id]->height();
        sync_.textures[id.id].stride = textures_[id.id]->stride();
        sync_.textures[id.id].pixels = textures_[id.id]->pixels();
    }
    sync_.dirty_textures.clear();

    // TODO : implement instances
    if (!sync_.comp_scene) {
        //sync_.scene_data->instances     = sync_.instances.data();
        //sync_.scene_data->num_instances = sync_.instances.size();
        sync_.scene_data->meshes        = sync_.meshes.data();
        sync_.scene_data->num_meshes    = sync_.meshes.size();
        sync_.scene_data->textures      = sync_.textures.data();
        sync_.scene_data->num_textures  = sync_.textures.size();

        sync_.comp_scene.reset(compile_scene(sync_.scene_data.get()));
    } else {
        // TODO : update scene. Pass information on which mesh has changed to impala
        // Refit meshes that keep the same connectivity, rebuild others.
        assert(0 && "Not implemented");
    }

    dirty_ = false;
}

} // namespace imba

