#include "tri.h"
#include "float3.h"

namespace imba {

inline float3 clip_line(int axis, float plane, const float3& a, const float3& b) {
    const float t = (plane - a[axis]) / (b[axis] - a[axis]);
    return a + t * (b - a);
}

BBox Tri::clipped_bbox(int axis, float min, float max) const {
    float3 points[10];
    float3* cur_poly = &points[0];
    float3* next_poly = &points[5];
    int cur_count = 3;
    int next_count = 0;

    cur_poly[0] = v0;
    cur_poly[1] = v1;
    cur_poly[2] = v2;

    // Clip against min
    for (int i = 0, j = cur_count - 1; i < cur_count; j = i, i++) {
        const float3& prev = cur_poly[j];
        const float3& next = cur_poly[i];
        if (next[axis] > min) {
            if (prev[axis] < min)
                next_poly[next_count++] = clip_line(axis, min, prev, next);

            next_poly[next_count++] = next;
        }
    }

    std::swap(cur_poly, next_poly);
    cur_count = next_count;
    next_count = 0;

    // Clip against max
    for (int i = 0, j = cur_count - 1; i < cur_count; j = i, i++) {
        const float3& prev = cur_poly[j];
        const float3& next = cur_poly[i];
        if (next[axis] < max) {
            if (prev[axis] > max)
                next_poly[next_count++] = clip_line(axis, max, prev, next);

            next_poly[next_count++] = next;
        }
    }

    // Find the bounding box
    BBox bb(next_poly[0], next_poly[0]);
    for (int i = 1; i < next_count; i++)
        bb = extend(bb, next_poly[i]);

    return bb;
}

} // namespace imba
