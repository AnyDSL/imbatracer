#ifndef IMBA_INTERSECTION_H
#define IMBA_INTERSECTION_H

namespace imba {

class Material;

struct Intersection {
    float3 pos;
    float3 out_dir; // Inverted direction of the ray at the hitpoint

    float3 normal;
    float2 uv;
    float3 geom_normal;

    float area;
    int mat;
    float d_sqr; ///< Squared distance from the previous vertex or one if there is none.
};

} // namespace imba

#endif // IMBA_INTERSECTION_H
