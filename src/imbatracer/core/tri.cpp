#include "tri.h"
#include "float3.h"

namespace imba {

inline float3 clip_edge(int axis, float plane, const float3& v, const float3& edge) {
    const float t = (plane - v[axis]) / (edge[axis]);
    return v + t * edge;
}

void Tri::compute_clipped_bbox(BBox& bb, int axis, float min, float max) const {
    bb = BBox::empty();

    const float3& e0 = v1 - v0;
    const float3& e1 = v2 - v1;
    const float3& e2 = v0 - v2;

    if (v0[axis] < min) {
        if (v1[axis] >= min) bb.extend(clip_edge(axis, min, v0, e0));
        if (v2[axis] >= min) bb.extend(clip_edge(axis, min, v2, e2));
    } else if (v0[axis] > max) {
        if (v1[axis] <= max) bb.extend(clip_edge(axis, max, v0, e0));
        if (v2[axis] <= max) bb.extend(clip_edge(axis, max, v2, e2));
    } else {
        bb.extend(v0);
    }

    if (v1[axis] < min) {
        if (v2[axis] >= min) bb.extend(clip_edge(axis, min, v1, e1));
        if (v0[axis] >= min) bb.extend(clip_edge(axis, min, v0, e0));
    } else if (v1[axis] > max) {
        if (v2[axis] <= max) bb.extend(clip_edge(axis, max, v1, e1));
        if (v0[axis] <= max) bb.extend(clip_edge(axis, max, v0, e0));
    } else {
        bb.extend(v1);
    }

    if (v2[axis] < min) {
        if (v0[axis] >= min) bb.extend(clip_edge(axis, min, v2, e2));
        if (v1[axis] >= min) bb.extend(clip_edge(axis, min, v1, e1));
    } else if (v2[axis] > max) {
        if (v0[axis] <= max) bb.extend(clip_edge(axis, max, v2, e2));
        if (v1[axis] <= max) bb.extend(clip_edge(axis, max, v1, e1));
    } else {
        bb.extend(v2);
    }
}

} // namespace imba
