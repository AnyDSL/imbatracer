#ifndef IMBA_OBJ_LOADER_HPP
#define IMBA_OBJ_LOADER_HPP

#include "scene_loader.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace imba {

/// Fast, robust OBJ file parser. Supports relative vertex indices.
class ObjLoader : public SceneLoader {
public:
    ObjLoader(TextureLoaderManager* manager = nullptr)
        : SceneLoader(manager)
    {}

    virtual bool check_format(const Path& path) override;
    virtual bool load_file(const Path& path, Scene& scene, Logger* logger = nullptr) override;

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

    struct ObjFile {
        std::vector<Object>      objects;
        std::vector<Vertex>      vertices;
        std::vector<Normal>      normals;
        std::vector<Texcoord>    texcoords;
        std::vector<std::string> materials;
        std::vector<std::string> mtl_libs;
    };

    struct Material {
        float ka[3];
        float kd[3];
        float ks[3];
        float ns;
        float d;
        int illum;
        std::string map_ka;
        std::string map_kd;
        std::string map_ks;
        std::string map_bump;
    };

    bool parse_obj_stream(std::istream& stream, ObjFile& file, Logger* logger);
    bool parse_mtl_stream(std::istream& stream, std::unordered_map<std::string, Material>& materials, Logger* logger);
    bool read_index(char** ptr, Index& idx);
};

}

#endif // IMBA_OBJ_LOADER_HPP

