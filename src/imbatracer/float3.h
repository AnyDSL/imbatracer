#ifndef FLOAT3_H
#define FLOAT3_H

#include <cmath>

#include "float2.h"

namespace imba {

struct float3 {
    float x, y, z;
    float3() {}
    float3(float x) : x(x), y(x), z(x) {}
    float3(float x, float y, float z) : x(x), y(y), z(z) {}
    float3(const float2& f, float z) : x(f.x), y(f.y), z(z) {}
    float3(float x, const float2& f) : x(x), y(f.x), z(f.y) {}

    float operator [] (int axis) const { return *(&x + axis); }
    float& operator [] (int axis) { return *(&x + axis); }
};

inline float3 operator * (float a, const float3& b) {
    return float3(a * b.x, a * b.y, a * b.z);
}

inline float3 operator * (const float3& a, float b) {
    return float3(a.x * b, a.y * b, a.z * b);
}

inline float3 operator - (const float3& a, const float3& b) {
    return float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline float3 operator + (const float3& a, const float3& b) {
    return float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline float3 operator * (const float3& a, const float3& b) {
    return float3(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline float3 cross(const float3& a, const float3& b) {
    return float3(a.y * b.z - a.z * b.y,
                  a.z * b.x - a.x * b.z, 
                  a.x * b.y - a.y * b.x);
}

inline float3 min(const float3& a, const float3& b) {
    return float3(a.x < b.x ? a.x : b.x,
                  a.y < b.y ? a.y : b.y,
                  a.z < b.z ? a.z : b.z);
}

inline float3 max(const float3& a, const float3& b) {
    return float3(a.x > b.x ? a.x : b.x,
                  a.y > b.y ? a.y : b.y,
                  a.z > b.z ? a.z : b.z);
}

inline float dot(const float3& a, const float3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float3 normalize(const float3& a) {
    float lensq = dot(a, a);
    return a * (1.0f / sqrtf(lensq));
}

} // namespace imba

#endif // FLOAT3_H
