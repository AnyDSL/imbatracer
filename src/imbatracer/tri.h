#ifndef IMBA_TRI_H
#define IMBA_TRI_H

#include "float3.h"
#include "bbox.h"

namespace imba {

struct Tri {
    float3 v0, v1, v2;

    Tri() {}
    Tri(const float3& v0, const float3& v1, const float3& v2)
        : v0(v0), v1(v1), v2(v2)
    {}

    float area() const { return length(cross(v1 - v0, v2 - v0)) / 2; }
    BBox bbox() const { return BBox(min(v0, min(v1, v2)), max(v0, max(v1, v2))); }

    /// Clips a triangle along one axis and returns the resulting bounding box
    BBox clipped_bbox(int axis, float min, float max) const;
};

} // namespace imba

#endif // IMBA_TRI_H
