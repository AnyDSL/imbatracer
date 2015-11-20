#ifndef BUILD_SCENE_H
#define BUILD_SCENE_H

#include "../core/mesh.h"
#include "../render/scene.h"
#include "../render/light.h"
#include "../render/texture_sampler.h"
#include "../loaders/path.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace imba {

bool build_scene(const Path& path, Mesh& scene, MaterialContainer& scene_materials, TextureContainer& textures, 
                 std::vector<int>& triangle_material_ids, std::vector<float2>& texcoords, LightContainer& lights);

}

#endif // BUILD_SCENE_H
