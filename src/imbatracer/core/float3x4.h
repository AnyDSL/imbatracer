#ifndef IMBA_FLOAT3X4_H
#define IMBA_FLOAT3X4_H

#include "float4.h"
#include "bbox.h"
#include "bsphere.h"

namespace imba {

struct float3x4 {
    float4 rows[3];

    float3x4() {}
    float3x4(const float4& r0, const float4& r1, const float4& r2) : rows{r0, r1, r2} {}
    explicit float3x4(const float4x4& mat) : rows { mat[0], mat[1], mat[2] } {}

    const float4& operator [] (int row) const { return rows[row]; }
    float4& operator [] (int row) { return rows[row]; }

    static inline float3x4 identity() {
        return float3x4(float4(1.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 1.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 1.0f, 0.0f));
    }
};

inline float3x4 operator * (const float3x4& a, float b) {
    return float3x4(a.rows[0] * b, a.rows[1] * b, a.rows[2] * b);
}

inline float3x4 operator * (float a, const float3x4& b) {
    return b * a;
}

inline float3 operator * (const float3x4& a, const float4& b) {
    return float3(dot(a[0], b),
                  dot(a[1], b),
                  dot(a[2], b));
}

inline float4 operator * (const float3& a, const float3x4& b) {
    return float4(b[0][0] * a[0] + b[1][0] * a[1] + b[2][0] * a[2],
                  b[0][1] * a[0] + b[1][1] * a[1] + b[2][1] * a[2],
                  b[0][2] * a[0] + b[1][2] * a[1] + b[2][2] * a[2],
                  b[0][3] * a[0] + b[1][3] * a[1] + b[2][3] * a[2]);
}

inline float3x4 abs(const float3x4& a) {
    return float3x4(abs(a[0]), abs(a[1]), abs(a[2]));
}

inline BBox transform(const float3x4& m, const BBox& bb) {
    auto c = m * float4((bb.max + bb.min) * 0.5f, 1.0f);
    auto e = abs(m) * float4((bb.max - bb.min) * 0.5f, 0.0f);
    return BBox(c - e, c + e);
}

inline BSphere transform(const float3x4& m, const BSphere& s) {
    auto c = m * float4(s.center, 1.0f);
    auto rx = length(m * float4(s.radius, 0.0f, 0.0f, 0.0f));
    auto ry = length(m * float4(0.0f, s.radius, 0.0f, 0.0f));
    auto rz = length(m * float4(0.0f, 0.0f, s.radius, 0.0f));
    return BSphere(c, std::max(rx, std::max(ry, rz)));
}

} // namespace imba

#endif // IMBA_FLOAT3X4_H
