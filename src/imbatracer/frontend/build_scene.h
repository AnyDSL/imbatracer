#ifndef BUILD_SCENE_H
#define BUILD_SCENE_H

#include "imbatracer/core/mesh.h"
#include "imbatracer/render/scene.h"
#include "imbatracer/render/light.h"
#include "imbatracer/render/texture_sampler.h"
#include "imbatracer/loaders/path.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace imba {

bool build_scene(const Path& path, Scene& scene, float3& cam_pos, float3& cam_dir, float3& cam_up);

}

#endif // BUILD_SCENE_H
