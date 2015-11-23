#ifndef IMBA_LOAD_ZMOD_H
#define IMBA_LOAD_ZMOD_H

#include <vector>

#include "../core/float3.h"
#include "../core/float2.h"
#include "path.h"

namespace imba {

namespace zmod {

struct File {
    std::vector<float3> vertices;
    std::vector<float3> normals;
    std::vector<float2> texcoords;
    std::vector<int32_t> indices;
    std::vector<int32_t> mat_ids;
    std::vector<std::string> mat_names;
};

} // namespace zmod

bool load_zmod(const Path&, zmod::File&);

} // namespace imba

#endif // IMBA_LOAD_ZMOD_H
