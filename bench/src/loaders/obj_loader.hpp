#ifndef IMBA_OBJ_LOADER_HPP
#define IMBA_OBJ_LOADER_HPP

#include "scene_loader.hpp"

namespace imba {

/// Fast, robust OBJ file parser. Supports relative vertex indices.
class ObjLoader : public SceneLoader {
public:
    virtual bool load_scene(const std::string& working_dir,
                            const std::string& name, Scene& scene,
                            Logger* logger = nullptr) override;

private:
    struct Index {
        int v, n, t;
    };

    struct Face {
        static constexpr int max_indices = 8;
        Index indices[max_indices];
        int index_count;
        int material;
    };

    struct Vertex {
        float x, y, z;
    };

    struct Normal {
        float x, y, z;
    };

    struct Texcoord {
        float u, v;
    };

    struct Group {
        std::vector<Face> faces;
    };

    struct Object {
        std::vector<Group> groups;
    };

    struct File {
        std::vector<Object>      objects;
        std::vector<Vertex>      vertices;
        std::vector<Normal>      normals;
        std::vector<Texcoord>    texcoords;
        std::vector<std::string> materials;
        std::vector<std::string> mtl_libs;
    };

    bool parse_stream(std::istream& stream, File& file, Logger* logger);
    bool read_index(char** ptr, Index& idx);
};

}

#endif // IMBA_OBJ_LOADER_HPP

