#ifndef IMBA_FLOAT2_H
#define IMBA_FLOAT2_H

#include <cmath>
#include "float2.h"

namespace imba {

struct float2 {
    float x, y, z;
    float2() {}
    float2(float x) : x(x), y(x) {}
    float2(float x, float y) : x(x), y(y) {}

    float operator [] (int axis) const { return *(&x + axis); }
    float& operator [] (int axis) { return *(&x + axis); }
};

inline float2 operator * (float a, const float2& b) {
    return float2(a * b.x, a * b.y);
}

inline float2 operator * (const float2& a, float b) {
    return float2(a.x * b, a.y * b);
}

inline float2 operator - (const float2& a, const float2& b) {
    return float2(a.x - b.x, a.y - b.y);
}

inline float2 operator + (const float2& a, const float2& b) {
    return float2(a.x + b.x, a.y + b.y);
}

inline float2 operator * (const float2& a, const float2& b) {
    return float2(a.x * b.x, a.y * b.y);
}

inline float2 min(const float2& a, const float2& b) {
    return float2(a.x < b.x ? a.x : b.x,
                  a.y < b.y ? a.y : b.y);
}

inline float2 max(const float2& a, const float2& b) {
    return float2(a.x > b.x ? a.x : b.x,
                  a.y > b.y ? a.y : b.y);
}

inline float dot(const float2& a, const float2& b) {
    return a.x * b.x + a.y * b.y;
}

inline float length(const float2& a) {
    return sqrtf(dot(a, a));
}

inline float2 normalize(const float2& a) {
    return a * (1.0f / length(a));
}

} // namespace imba

#endif // IMBA_FLOAT2_H
