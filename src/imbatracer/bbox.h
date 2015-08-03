#ifndef IMBA_BBOX_H
#define IMBA_BBOX_H

#include <cfloat>
#include <algorithm>

#include "float3.h"

namespace imba {

struct BBox {
    float3 min, max;
    BBox() {}
    BBox(const float3& f) : min(f), max(f) {}
    BBox(const float3& min, const float3& max) : min(min), max(max) {}

    static BBox empty() { return BBox(float3(FLT_MAX), float3(-FLT_MAX)); }
    static BBox full() { return BBox(float3(-FLT_MAX), float3(FLT_MAX)); }
};

inline BBox extend(const BBox& bb, const float3& f) {
    return BBox(min(bb.min, f), max(bb.max, f));
}

inline BBox extend(const BBox& a, const BBox& b) {
    return BBox(min(a.min, b.min), max(a.max, b.max));
}

inline BBox overlap(const BBox& a, const BBox& b) {
    return BBox(max(a.min, b.min), min(a.max, b.max));
}

inline float half_area(const BBox& bb) {
    const float3 len = bb.max - bb.min;
    return std::max(len.x * (len.y + len.z) + len.y * len.z, 0.0f);
}

inline bool is_empty(const BBox& bb) {
    return bb.min.x > bb.max.x || bb.min.y > bb.max.y || bb.min.z > bb.max.z;
}

inline bool is_inside(const BBox& bb, const float3& f) {
    return f.x >= bb.min.x && f.y >= bb.min.y && f.z >= bb.min.z &&
           f.x <= bb.max.x && f.y <= bb.max.y && f.z <= bb.max.z;
}

} // namespace imba

#endif // IMBA_BBOX_H
