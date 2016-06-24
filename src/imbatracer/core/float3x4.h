#ifndef IMBA_FLOAT3X4
#define IMBA_FLOAT3X4

#include "float4.h"

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

inline float4 transform(const float3x4& a, const float4& b) {
    return float4(dot(a.rows[0], b),
                  dot(a.rows[1], b),
                  dot(a.rows[2], b),
                  b.w);
}

inline float3 transform_point(const float3x4& a, const float3& b) {
    float4 t = transform(a, float4(b, 1.0f));
    return float3(t.x, t.y, t.z);
}

inline float3 transform_vector(const float3x4& a, const float3& b) {
    float4 t = transform(a, float4(b, 0.0f));
    return float3(t.x, t.y, t.z);
}

inline float4 operator* (const float3& a, const float3x4& b) {
    float4 res (0.0f);
    for (int i = 0; i < 4; ++i){
        for (int j = 0; j < 3; ++j) {
            res[i] += a[j] * b[j][i];
        }
    }
    return res;
}

inline float3x4 abs(const float3x4& a) {
    float3x4 res;
    for (int i = 0; i < 3; ++i)
        res[i] = abs(a[i]);
    return res;
}

}

#endif