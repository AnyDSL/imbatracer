#ifndef IMBA_INTERSECTION_H
#define IMBA_INTERSECTION_H

namespace imba {

class Material;

struct Intersection {
    float3 pos;
    float3 out_dir; // Inverted direction of the ray at the hitpoint
    float distance;

    float3 normal;
    float2 uv;
    float3 geom_normal;

    float3 u_tangent;
    float3 v_tangent;

    Material* mat;
};

} // namespace imba

#endif
