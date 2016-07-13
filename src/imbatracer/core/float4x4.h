#ifndef IMBA_FLOAT4X4_H
#define IMBA_FLOAT4X4_H

#include "float4.h"

namespace imba {

struct float4x4 {
    float4 rows[4];

    float4x4() {}
    float4x4(const float4& r0, const float4& r1, const float4& r2, const float4& r3) : rows{r0, r1, r2, r3} {}

    const float4& operator [] (int row) const { return rows[row]; }
    float4& operator [] (int row) { return rows[row]; }

    static inline float4x4 identity() {
        return float4x4(float4(1.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 1.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 1.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static inline float4x4 zero() {
        return float4x4(float4(0.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 0.0f));
    }
};

inline float4x4 perspective(float fov, float aspect, float near, float far) {
    // Camera points towards -z.  0 < near < far.
    // Matrix maps z range [-near, -far] to [-1, 1], after homogeneous division.
    const float f_h =   1.0f / std::tan(fov * pi / 360.0f);
    const float f_v = aspect / std::tan(fov * pi / 360.0f);
    const float d = 1.0f / (near - far);

    float4x4 r;
    r[0][0] = f_h;  r[0][1] = 0.0f; r[0][2] = 0.0f;             r[0][3] = 0.0f;
    r[1][0] = 0.0f; r[1][1] = -f_v; r[1][2] = 0.0f;             r[1][3] = 0.0f;
    r[2][0] = 0.0f; r[2][1] = 0.0f; r[2][2] = (near + far) * d; r[2][3] = 2.0f * near * far * d;
    r[3][0] = 0.0f; r[3][1] = 0.0f; r[3][2] = -1.0f;            r[3][3] = 0.0f;

    return r;
}

inline float4x4 scale(float x, float y, float z, float w = 1.0f) {
    return float4x4(float4(   x, 0.0f, 0.0f, 0.0f),
                    float4(0.0f,    y, 0.0f, 0.0f),
                    float4(0.0f, 0.0f,    z, 0.0f),
                    float4(0.0f, 0.0f, 0.0f,    w));
}

inline float4x4 translate(float x, float y, float z) {
    return float4x4(float4(1.0f, 0.0f, 0.0f,    x),
                    float4(0.0f, 1.0f, 0.0f,    y),
                    float4(0.0f, 0.0f, 1.0f,    z),
                    float4(0.0f, 0.0f, 0.0f, 1.0f));
}

inline float4x4 rotate_x(float angle) {
    return float4x4(float4(1.0f,         0.0f,        0.0f, 0.0f),
                    float4(0.0f,  cosf(angle), sinf(angle), 0.0f),
                    float4(0.0f, -sinf(angle), cosf(angle), 0.0f),
                    float4(0.0f,         0.0f,        0.0f, 1.0f));
}

inline float4x4 rotate_y(float angle) {
    return float4x4(float4(cosf(angle), 0.0f, -sinf(angle), 0.0f),
                    float4(       0.0f, 1.0f,         0.0f, 0.0f),
                    float4(sinf(angle), 0.0f,  cosf(angle), 0.0f),
                    float4(       0.0f, 0.0f,         0.0f, 1.0f));
}

inline float4x4 rotate_z(float angle) {
    return float4x4(float4( cosf(angle), sinf(angle), 0.0f, 0.0f),
                    float4(-sinf(angle), cosf(angle), 0.0f, 0.0f),
                    float4(        0.0f,        0.0f, 1.0f, 0.0f),
                    float4(        0.0f,        0.0f, 0.0f, 1.0f));
}

inline float4x4 euler(float x, float y, float z);

inline float4x4 euler(const float3& v) {
    return euler(v.x, v.y, v.z);
}

inline float determinant(const float4x4& a) {
    float m0 = a[1][1] * a[2][2] * a[3][3] - a[1][1] * a[2][3] * a[3][2] - a[2][1] * a[1][2] * a[3][3] +
               a[2][1] * a[1][3] * a[3][2] + a[3][1] * a[1][2] * a[2][3] - a[3][1] * a[1][3] * a[2][2];

    float m1 = -a[1][0] * a[2][2] * a[3][3] + a[1][0] * a[2][3] * a[3][2] + a[2][0] * a[1][2] * a[3][3] -
               a[2][0] * a[1][3] * a[3][2] - a[3][0] * a[1][2] * a[2][3] + a[3][0] * a[1][3] * a[2][2];

    float m2 = a[1][0] * a[2][1] * a[3][3] - a[1][0] * a[2][3] * a[3][1] - a[2][0] * a[1][1] * a[3][3] +
               a[2][0] * a[1][3] * a[3][1] + a[3][0] * a[1][1] * a[2][3] - a[3][0] * a[1][3] * a[2][1];

    float m3 = -a[1][0] * a[2][1] * a[3][2] + a[1][0] * a[2][2] * a[3][1] + a[2][0] * a[1][1] * a[3][2] -
               a[2][0] * a[1][2] * a[3][1] - a[3][0] * a[1][1] * a[2][2] + a[3][0] * a[1][2] * a[2][1];

    float det = a[0][0] * m0 + a[0][1] * m1 + a[0][2] * m2 + a[0][3] * m3;

    return det;
}

inline float4x4 transpose(const float4x4& a) {
    return float4x4(float4(a.rows[0][0], a.rows[1][0], a.rows[2][0], a.rows[3][0]),
                    float4(a.rows[0][1], a.rows[1][1], a.rows[2][1], a.rows[3][1]),
                    float4(a.rows[0][2], a.rows[1][2], a.rows[2][2], a.rows[3][2]),
                    float4(a.rows[0][3], a.rows[1][3], a.rows[2][3], a.rows[3][3]));
}

inline float4x4 operator * (const float4x4& a, const float4x4& b) {
    float4x4 t = transpose(b);
    return float4x4(float4(dot(a[0], t[0]), dot(a[0], t[1]), dot(a[0], t[2]), dot(a[0], t[3])),
                    float4(dot(a[1], t[0]), dot(a[1], t[1]), dot(a[1], t[2]), dot(a[1], t[3])),
                    float4(dot(a[2], t[0]), dot(a[2], t[1]), dot(a[2], t[2]), dot(a[2], t[3])),
                    float4(dot(a[3], t[0]), dot(a[3], t[1]), dot(a[3], t[2]), dot(a[3], t[3])));
}

inline float4x4 operator * (const float4x4& a, float b) {
    return float4x4(a.rows[0] * b, a.rows[1] * b, a.rows[2] * b, a.rows[3] * b);
}

inline float4x4 operator * (float a, const float4x4& b) {
    return b * a;
}

inline float4 operator * (const float4x4& a, const float4& b) {
    return float4(dot(a[0], b),
                  dot(a[1], b),
                  dot(a[2], b),
                  dot(a[3], b));
}

inline float4 operator * (const float4& a, const float4x4& b) {
    return float4(b[0][0] * a[0] + b[1][0] * a[1] + b[2][0] * a[2] + b[3][0] * a[3],
                  b[0][1] * a[0] + b[1][1] * a[1] + b[2][1] * a[2] + b[3][1] * a[3],
                  b[0][2] * a[0] + b[1][2] * a[1] + b[2][2] * a[2] + b[3][2] * a[3],
                  b[0][3] * a[0] + b[1][3] * a[1] + b[2][3] * a[2] + b[3][3] * a[3]);
}

inline float4x4 invert(const float4x4& a) {
    float4x4 result;

    //Taken from http://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    result[0][0] = a[1][1] * a[2][2] * a[3][3] - a[1][1] * a[2][3] * a[3][2] - a[2][1] * a[1][2] * a[3][3] +
                   a[2][1] * a[1][3] * a[3][2] + a[3][1] * a[1][2] * a[2][3] - a[3][1] * a[1][3] * a[2][2];

    result[1][0] = -a[1][0] * a[2][2] * a[3][3] + a[1][0] * a[2][3] * a[3][2] + a[2][0] * a[1][2] * a[3][3] -
                   a[2][0] * a[1][3] * a[3][2] - a[3][0] * a[1][2] * a[2][3] + a[3][0] * a[1][3] * a[2][2];

    result[2][0] = a[1][0] * a[2][1] * a[3][3] - a[1][0] * a[2][3] * a[3][1] - a[2][0] * a[1][1] * a[3][3] +
                   a[2][0] * a[1][3] * a[3][1] + a[3][0] * a[1][1] * a[2][3] - a[3][0] * a[1][3] * a[2][1];

    result[3][0] = -a[1][0] * a[2][1] * a[3][2] + a[1][0] * a[2][2] * a[3][1] + a[2][0] * a[1][1] * a[3][2] -
                   a[2][0] * a[1][2] * a[3][1] - a[3][0] * a[1][1] * a[2][2] + a[3][0] * a[1][2] * a[2][1];

    float det = a[0][0] * result[0][0] + a[0][1] * result[1][0] + a[0][2] * result[2][0] + a[0][3] * result[3][0];

    if (det == 0)
        return float4x4::zero();

    result[0][1] = -a[0][1] * a[2][2] * a[3][3] + a[0][1] * a[2][3] * a[3][2] + a[2][1] * a[0][2] * a[3][3] -
                   a[2][1] * a[0][3] * a[3][2] - a[3][1] * a[0][2] * a[2][3] + a[3][1] * a[0][3] * a[2][2];

    result[1][1] = a[0][0] * a[2][2] * a[3][3] - a[0][0] * a[2][3] * a[3][2] - a[2][0] * a[0][2] * a[3][3] +
                   a[2][0] * a[0][3] * a[3][2] + a[3][0] * a[0][2] * a[2][3] - a[3][0] * a[0][3] * a[2][2];

    result[2][1] = -a[0][0] * a[2][1] * a[3][3] + a[0][0] * a[2][3] * a[3][1] + a[2][0] * a[0][1] * a[3][3] -
                   a[2][0] * a[0][3] * a[3][1] - a[3][0] * a[0][1] * a[2][3] + a[3][0] * a[0][3] * a[2][1];

    result[3][1] = a[0][0] * a[2][1] * a[3][2] - a[0][0] * a[2][2] * a[3][1] - a[2][0] * a[0][1] * a[3][2] +
                   a[2][0] * a[0][2] * a[3][1] + a[3][0] * a[0][1] * a[2][2] - a[3][0] * a[0][2] * a[2][1];

    result[0][2] = a[0][1] * a[1][2] * a[3][3] - a[0][1] * a[1][3] * a[3][2] - a[1][1] * a[0][2] * a[3][3] +
                   a[1][1] * a[0][3] * a[3][2] + a[3][1] * a[0][2] * a[1][3] - a[3][1] * a[0][3] * a[1][2];

    result[1][2] = -a[0][0] * a[1][2] * a[3][3] + a[0][0] * a[1][3] * a[3][2] + a[1][0] * a[0][2] * a[3][3] -
                   a[1][0] * a[0][3] * a[3][2] - a[3][0] * a[0][2] * a[1][3] + a[3][0] * a[0][3] * a[1][2];

    result[2][2] = a[0][0] * a[1][1] * a[3][3] - a[0][0] * a[1][3] * a[3][1] - a[1][0] * a[0][1] * a[3][3] +
                   a[1][0] * a[0][3] * a[3][1] + a[3][0] * a[0][1] * a[1][3] - a[3][0] * a[0][3] * a[1][1];

    result[3][2] = -a[0][0] * a[1][1] * a[3][2] + a[0][0] * a[1][2] * a[3][1] + a[1][0] * a[0][1] * a[3][2] -
                   a[1][0] * a[0][2] * a[3][1] - a[3][0] * a[0][1] * a[1][2] + a[3][0] * a[0][2] * a[1][1];

    result[0][3] = -a[0][1] * a[1][2] * a[2][3] + a[0][1] * a[1][3] * a[2][2] + a[1][1] * a[0][2] * a[2][3] -
                   a[1][1] * a[0][3] * a[2][2] - a[2][1] * a[0][2] * a[1][3] + a[2][1] * a[0][3] * a[1][2];

    result[1][3] = a[0][0] * a[1][2] * a[2][3] - a[0][0] * a[1][3] * a[2][2] - a[1][0] * a[0][2] * a[2][3] +
                   a[1][0] * a[0][3] * a[2][2] + a[2][0] * a[0][2] * a[1][3] - a[2][0] * a[0][3] * a[1][2];

    result[2][3] = -a[0][0] * a[1][1] * a[2][3] + a[0][0] * a[1][3] * a[2][1] + a[1][0] * a[0][1] * a[2][3] -
                   a[1][0] * a[0][3] * a[2][1] - a[2][0] * a[0][1] * a[1][3] + a[2][0] * a[0][3] * a[1][1];

    result[3][3] = a[0][0] * a[1][1] * a[2][2] - a[0][0] * a[1][2] * a[2][1] - a[1][0] * a[0][1] * a[2][2] +
                   a[1][0] * a[0][2] * a[2][1] + a[2][0] * a[0][1] * a[1][2] - a[2][0] * a[0][2] * a[1][1];

    result = result * (1.0f / det);
    return result;
}

inline float4x4 abs(const float4x4& a) {
    return float4x4(abs(a[0]), abs(a[1]), abs(a[2]), abs(a[3]));
}

inline float4x4 euler(float x, float y, float z) {
    return rotate_x(x) * rotate_y(y) * rotate_z(z);
}

} // namespace imba

#endif // IMBA_FLOAT4X4_H
