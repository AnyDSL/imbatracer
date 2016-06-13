#ifndef IMBA_BSPHERE_H
#define IMBA_BSPHERE_H

#include "float3.h"

namespace imba {

// Bounding sphere utility class. Stores a precomputed inverse-square radius value.
struct BSphere {
    float3 center;
    float radius;
    float inv_radius_sqr;

    BSphere() {}
    BSphere(const float3& c, float r)
        : center(c)
        , radius(r)
        , inv_radius_sqr(1.0f / (r * r))
    {}

    float distance(const float3& p) const { return length(p - center) - radius; }
    float distancesqr(const float3& p) const { return lensqr(p - center) - radius * radius; }
};

} // namespace imba

#endif // IMBA_BSPHERE_H
