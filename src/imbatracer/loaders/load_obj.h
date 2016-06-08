#ifndef IMBA_LOAD_OBJ_H
#define IMBA_LOAD_OBJ_H

#include <vector>
#include <string>
#include <unordered_map>

#include "../core/vector.h"
#include "path.h"

namespace imba {

namespace obj {

struct Index {
    int v, n, t;
};

struct Face {
    static constexpr int max_indices = 8;
    Index indices[max_indices];
    int index_count;
    int material;
};

struct Group {
    std::vector<Face> faces;
};

struct Object {
    std::vector<Group> groups;
};

struct Material {
    float3 ka;
    float3 kd;
    float3 ks;
    float3 ke;
    float ns;
    float ni;
    float3 tf;
    float tr;
    float d;
    int illum;
    std::string map_ka;
    std::string map_kd;
    std::string map_ks;
    std::string map_ke;
    std::string map_bump;
    std::string map_d;
};

struct File {
    std::vector<Object>      objects;
    std::vector<float3>      vertices;
    std::vector<float3>      normals;
    std::vector<float2>      texcoords;
    std::vector<std::string> materials;
    std::vector<std::string> mtl_libs;
};

typedef std::unordered_map<std::string, Material> MaterialLib;

}

bool load_obj(const Path&, obj::File&);
bool load_mtl(const Path&, obj::MaterialLib&);

} // namespace imba

#endif // IMBA_LOAD_OBJ_H
