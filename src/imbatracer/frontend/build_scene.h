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

bool build_scene(const Path& path, Scene& scene, float3& cam_pos, float3& cam_dir, float3& cam_up);

}

#endif // BUILD_SCENE_H
