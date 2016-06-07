#ifndef IMBA_RGB_H
#define IMBA_RGB_H

#include "float4.h"
#include "atomic_vector.h"

namespace imba {

struct rgba;

/// Stores RGB color data
struct rgb : public float3 {
    rgb() {}
    rgb(const float3& f) : float3(f) {}
    explicit rgb(float x) : float3(x) {}
    explicit rgb(const rgba& f);
    rgb(float x, float y, float z) : float3(x, y, z) {}

    operator float3& () { return *this; }
};

struct rgba : public float4 {
    rgba() {}
    rgba(const float4& f) : float4(f) {}
    explicit rgba(float x) : float4(x) {}
    rgba(float x, float y, float z, float w) : float4(x, y, z, w) {}
    rgba(const rgb& f, float w) : float4(f.x, f.y, f.z, w) {}
    rgba(float x, const rgb& f) : float4(x, f.x, f.y, f.z) {}

    operator float4& () { return *this; }
};

inline rgb::rgb(const rgba& f) : float3(f) {}

inline bool is_black(const rgb& a) {
    return a.x <= 0.0f && a.y <= 0.0f && a.z <= 0.0f;
}

inline bool is_black(const rgba& a) {
    return a.x <= 0.0f && a.y <= 0.0f && a.z <= 0.0f;
}

inline rgb clamp(const rgb& val, const rgb& min, const rgb& max) {
    return rgb(clamp(val.x, min.x, max.x),
               clamp(val.y, min.y, max.y),
               clamp(val.z, min.z, max.z));
}

inline rgba clamp(const rgba& val, const rgba& min, const rgba& max) {
    return rgba(clamp(val.x, min.x, max.x),
                clamp(val.y, min.y, max.y),
                clamp(val.z, min.z, max.z),
                clamp(val.w, min.w, max.w));
}

using atomic_rgb  = AtomicVector<float, 3>;
using atomic_rgba = AtomicVector<float, 4>;

} // namespace imba

#endif