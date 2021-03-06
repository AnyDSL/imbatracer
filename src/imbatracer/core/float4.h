#ifndef IMBA_FLOAT4_H
#define IMBA_FLOAT4_H

#include <cmath>
#include "imbatracer/core/common.h"
#include "imbatracer/core/float3.h"
#include "imbatracer/core/float2.h"

namespace imba {

struct float4 {
    float x, y, z, w;
    float4() {}
    explicit float4(float x) : x(x), y(x), z(x), w(x) {}
    float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    float4(const float3& f, float w) : x(f.x), y(f.y), z(f.z), w(w) {}
    float4(float x, const float3& f) : x(x), y(f.x), z(f.y), w(f.z) {}
    float4(const float2& f, float z, float w) : x(f.x), y(f.y), z(z), w(w) {}
    float4(float x, const float2& f, float w) : x(x), y(f.x), z(f.y), w(w) {}
    float4(float x, float y, const float2& f) : x(x), y(y), z(f.x), w(f.y) {}
    float4(const float2& f, const float2& g) : x(f.x), y(f.y), z(g.x), w(g.y) {}

    float operator [] (int axis) const { return *(&x + axis); }
    float& operator [] (int axis) { return *(&x + axis); }

    float4& operator += (const float4& a) {
        x += a.x; y += a.y; z += a.z; w += a.w;
        return *this;
    }

    float4& operator *= (float a) {
        x *= a; y *= a; z *= a; w *= a;
        return *this;
    }

    float4& operator *= (const float4& a) {
        x *= a.x; y *= a.y; z *= a.z; w *= a.w;
        return *this;
    }
};

inline float3::float3(const float4& f) : x(f.x), y(f.y), z(f.z) {}
inline float2::float2(const float4& f) : x(f.x), y(f.y) {}

inline float4 operator * (float a, const float4& b) {
    return float4(a * b.x, a * b.y, a * b.z, a * b.w);
}

inline float4 operator * (const float4& a, float b) {
    return float4(a.x * b, a.y * b, a.z * b, a.w * b);
}

inline float4 operator / (const float4& a, float b) {
    return float4(a.x / b, a.y / b, a.z / b, a.w / b);
}

inline float4 operator - (const float4& a, const float4& b) {
    return float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

inline float4 operator - (const float4& a) {
    return float4(-a.x, -a.y, -a.z, -a.w);
}

inline float4 operator + (const float4& a, const float4& b) {
    return float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

inline float4 operator * (const float4& a, const float4& b) {
    return float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

inline float4 abs(const float4& a) {
    return float4(fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w));
}

inline float4 min(const float4& a, const float4& b) {
    return float4(a.x < b.x ? a.x : b.x,
                  a.y < b.y ? a.y : b.y,
                  a.z < b.z ? a.z : b.z,
                  a.w < b.w ? a.w : b.w);
}

inline float4 max(const float4& a, const float4& b) {
    return float4(a.x > b.x ? a.x : b.x,
                  a.y > b.y ? a.y : b.y,
                  a.z > b.z ? a.z : b.z,
                  a.w > b.w ? a.w : b.w);
}

inline float dot(const float4& a, const float4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline float lensqr(const float4& a) {
    return dot(a, a);
}

inline float length(const float4& a) {
    return sqrtf(dot(a, a));
}

inline float4 normalize(const float4& a) {
    return a * (1.0f / length(a));
}

inline float4 clamp(const float4& val, const float4& min, const float4& max) {
    return float4(clamp(val.x, min.x, max.x),
                  clamp(val.y, min.y, max.y),
                  clamp(val.z, min.z, max.z),
                  clamp(val.w, min.w, max.w));
}

} // namespace imba

#endif // IMBA_FLOAT4_H
