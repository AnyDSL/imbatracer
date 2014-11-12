#ifndef SCENE_LOADER_HPP
#define SCENE_LOADER_HPP

#include <memory>
#include <string>
#include <fstream>

#include "../scene/scene.hpp"
#include "../common/logger.hpp"

namespace imba {

/// Scene loader takes an input file as input and loads a scene object
/// from it. Different loaders can be implemented for different file formats.
class SceneLoader {
public:
    virtual ~SceneLoader() {}

    virtual bool load_scene(const std::string& working_dir,
                            const std::string& name, Scene& scene,
                            Logger* logger = nullptr) = 0;
};

} // namespace imba

#endif

