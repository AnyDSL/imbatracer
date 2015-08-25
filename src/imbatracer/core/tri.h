#ifndef IMBA_TRI_H
#define IMBA_TRI_H

#include "float3.h"
#include "bbox.h"
#include "common.h"

namespace imba {

struct Tri {
    float3 v0, v1, v2;

    Tri() {}
    Tri(const float3& v0, const float3& v1, const float3& v2)
        : v0(v0), v1(v1), v2(v2)
    {}

    float area() const { return length(cross(v1 - v0, v2 - v0)) / 2; }

    /// Computes the triangle bounding box.
    void compute_bbox(BBox& bb) const {
        bb.min = min(v0, min(v1, v2));
        bb.max = max(v0, max(v1, v2));
    }

    /// Clips the triangle along one axis and returns the resulting bounding box.
    void compute_clipped_bbox(BBox& bb, int axis, float min, float max) const {
        bb = BBox::empty();

        const float3& e0 = v1 - v0;
        const float3& e1 = v2 - v1;
        const float3& e2 = v0 - v2;

        const bool min0 = v0[axis] < min;
        const bool min1 = v1[axis] < min;
        const bool min2 = v2[axis] < min;

        const bool max0 = v0[axis] > max;
        const bool max1 = v1[axis] > max;
        const bool max2 = v2[axis] > max;

        if (!min0 & !max0) bb.extend(v0);
        if (!min1 & !max1) bb.extend(v1);
        if (!min2 & !max2) bb.extend(v2);

        if (min0 ^ min1) bb.extend(clip_edge(axis, min, v0, e0));
        if (min0 ^ min2) bb.extend(clip_edge(axis, min, v2, e2));
        if (min1 ^ min2) bb.extend(clip_edge(axis, min, v1, e1));

        if (max0 ^ max1) bb.extend(clip_edge(axis, max, v0, e0));
        if (max0 ^ max2) bb.extend(clip_edge(axis, max, v2, e2));
        if (max1 ^ max2) bb.extend(clip_edge(axis, max, v1, e1));
    }

private:
    static float3 clip_edge(int axis, float plane, const float3& p, const float3& edge) {
        const float t = (plane - p[axis]) / (edge[axis]);
        return p + t * edge;
    }
};

} // namespace imba

#endif // IMBA_TRI_H
