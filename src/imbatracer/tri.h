#ifndef TRI_H
#define TRI_H

#include "float3.h"
#include "bbox.h"

namespace imba {

struct Tri {
    float3 v0, v1, v2;

    Tri() {}
    Tri(const float3& v0, const float3& v1, const float3& v2)
        : v0(v0), v1(v1), v2(v2)
    {}
};

inline BBox bounding_box(const Tri& tri) {
    return BBox(min(tri.v0, min(tri.v1, tri.v2)),
                max(tri.v0, max(tri.v1, tri.v2)));
}

} // namespace imba

#endif // TRI_H
