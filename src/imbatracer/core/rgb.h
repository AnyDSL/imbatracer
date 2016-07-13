#ifndef IMBA_RGB_H
#define IMBA_RGB_H

#include "float4.h"
#include "atomic_vector.h"

namespace imba {

using rgb  = float3;
using rgba = float4;

inline bool is_black(const rgb& a) {
    return a.x <= 0.0f && a.y <= 0.0f && a.z <= 0.0f;
}

inline bool is_black(const rgba& a) {
    return a.x <= 0.0f && a.y <= 0.0f && a.z <= 0.0f;
}

using atomic_rgb  = AtomicVector<float, 3, rgb>;
using atomic_rgba = AtomicVector<float, 4, rgba>;

} // namespace imba

#endif