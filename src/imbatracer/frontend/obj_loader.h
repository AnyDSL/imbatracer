#ifndef IMBA_OBJ_LOADER_HPP
#define IMBA_OBJ_LOADER_HPP

#include "../core/mesh.h"
#include "../render/scene.h"
#include "../render/light.h"
#include "../render/texture_sampler.h"
#include "logger.h"
#include "path.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace imba {

/// Fast, robust OBJ file parser. Supports relative vertex indices.
class ObjLoader {
public:
    ObjLoader()
    {}

    bool check_format(const Path& path);
    bool load_file(const Path& path, Mesh& scene, MaterialContainer& scene_materials, TextureContainer& textures, 
                   std::vector<int>& triangle_material_ids, std::vector<float2>& texcoords, LightContainer& lights, Logger* logger = nullptr);

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
    
    struct ObjMaterial {
        float3 ka;
        float3 kd;
        float3 ks;
        float3 ke;
        float ns;
        float d;
        int illum;
        std::string map_ka;
        std::string map_kd;
        std::string map_ks;
        std::string map_ke;
        std::string map_bump;
        std::string map_d;
    };

    struct ObjFile {
        std::vector<Object>      objects;
        std::vector<Vertex>      vertices;
        std::vector<Normal>      normals;
        std::vector<Texcoord>    texcoords;
        std::vector<std::string> materials;
        std::vector<std::string> mtl_libs;
    };

    bool parse_obj_stream(std::istream& stream, ObjFile& file, Logger* logger);
    bool parse_mtl_stream(std::istream& stream, std::unordered_map<std::string, ObjMaterial>& materials, Logger* logger);
    bool read_index(char** ptr, Index& idx);
};

}

#endif // IMBA_OBJ_LOADER_HPP
