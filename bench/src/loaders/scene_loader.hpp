#ifndef IMBA_SCENE_LOADER_HPP
#define IMBA_SCENE_LOADER_HPP

#include <memory>
#include <string>
#include <fstream>

#include "../scene/scene.hpp"
#include "loader.hpp"
#include "image_loader.hpp"

namespace imba {

/// A scene loader takes a file as input and loads a scene object from it.
/// Different loaders can be implemented for different file formats.
class SceneLoader : public Loader<Scene> {
public:
    SceneLoader(TextureLoaderManager* manager)
        : manager_(manager)
    {}

    virtual ~SceneLoader() {}

    virtual bool check_format(const Path& path) = 0;
    virtual bool load_file(const Path& path, Scene& scene, Logger* logger) = 0;

protected:
    bool load_texture(const Path& path, Texture& image, Logger* logger) {
        return (manager_) ? manager_->load_file(path, image, logger) : false;
    }

    TextureLoaderManager* manager_;
};

typedef LoaderManager<Scene> SceneLoaderManager;

} // namespace imba

#endif // IMBA_SCENE_LOADER_HPP

