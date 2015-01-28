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
    sync_.scene_data->instances     = nullptr;
    sync_.scene_data->num_instances = 0;
    sync_.scene_data->lights       = nullptr;
    sync_.scene_data->num_lights   = 0;
    sync_.scene_data->materials    = nullptr;
    sync_.scene_data->materials    = 0;
}

Scene::~Scene() {
    for (auto mesh : meshes_) { delete mesh; }
    for (auto tex : textures_) { delete tex; }

    if (sync_.comp_scene)
        ::destroy_scene(sync_.scene_data.get(), sync_.comp_scene.get());
}

void Scene::compile() const {
    // Clear list of meshes to refit or rebuild
    sync_.to_refit.clear();
    sync_.to_rebuild.clear();

    // Synchronize meshes
    if (dirty_) {
        for (auto id : sync_.dirty_meshes) {
            if (sync_.meshes.size() <= id) {
                sync_.meshes.resize(id + 1);
            } else {
                // Need to update the existing acceleration structure
                if (sync_.meshes[id].num_tris == meshes_[id]->triangle_count()) {
                    // Assume that a refit is enough if only vertices moved
                    sync_.to_refit.push_back(id);
                } else {
                    // Rebuild is necessary
                    sync_.to_rebuild.push_back(id);
                }
            }
            
            sync_.meshes[id].vertices  = reinterpret_cast<::Vec3*>(meshes_[id]->vertices());
            sync_.meshes[id].normals   = reinterpret_cast<::Vec3*>(meshes_[id]->normals());
            sync_.meshes[id].texcoords = reinterpret_cast<::Vec2*>(meshes_[id]->texcoords());
            sync_.meshes[id].materials = reinterpret_cast<int*>(meshes_[id]->materials());
            sync_.meshes[id].indices   = reinterpret_cast<int*>(meshes_[id]->triangles());
            sync_.meshes[id].num_tris  = meshes_[id]->triangle_count();
        }
        sync_.dirty_meshes.clear();

        // Synchronize textures
        for (auto id : sync_.dirty_textures) {
            if (sync_.textures.size() <= id) sync_.textures.resize(id + 1);
            
            sync_.textures[id].width  = textures_[id]->width();
            sync_.textures[id].height = textures_[id]->height();
            sync_.textures[id].stride = textures_[id]->stride();
            sync_.textures[id].pixels = textures_[id]->pixels();
        }
        sync_.dirty_textures.clear();
    }

    const int new_meshes = sync_.meshes.size() - sync_.scene_data->num_meshes;
    const int new_insts  = sync_.instances.size() - sync_.scene_data->num_instances;

    sync_.scene_data->instances     = sync_.instances.data();
    sync_.scene_data->num_instances = sync_.instances.size();
    sync_.scene_data->meshes        = sync_.meshes.data();
    sync_.scene_data->num_meshes    = sync_.meshes.size();
    sync_.scene_data->textures      = sync_.textures.data();
    sync_.scene_data->num_textures  = sync_.textures.size();
    sync_.scene_data->materials     = sync_.materials.data();
    sync_.scene_data->num_materials = sync_.materials.size();
    sync_.scene_data->lights        = sync_.lights.data();
    sync_.scene_data->num_lights    = sync_.lights.size();

    if (!sync_.comp_scene) {
        sync_.comp_scene.reset(compile_scene(sync_.scene_data.get()));
    } else {
        ::SceneUpdate update_info;
        update_info.mesh_refit   = sync_.to_refit.data();
        update_info.num_refit    = sync_.to_refit.size();
        update_info.mesh_rebuild = sync_.to_rebuild.data();
        update_info.num_rebuild  = sync_.to_rebuild.size();
        update_info.inst_new     = new_insts;
        update_info.mesh_new     = new_meshes;

        update_scene(sync_.scene_data.get(), &update_info, sync_.comp_scene.get());
    }

    dirty_ = false;
}

} // namespace imba

