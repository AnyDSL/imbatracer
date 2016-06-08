#ifndef IMBA_MATRIX
#define IMBA_MATRIX

#include "vector.h"

namespace imba {

struct matrix {
    float4 rows[4];

    matrix() {}
    matrix(const float4& r0, const float4& r1, const float4& r2, const float4& r3) : rows{r0, r1, r2, r3} {}

    float4 operator [] (int row) const { return rows[row]; }
    float4& operator [] (int row) { return rows[row]; }
};

inline matrix identity_matrix() {
    return matrix(float4(1.0f, 0.0f, 0.0f, 0.0f),
                  float4(0.0f, 1.0f, 0.0f, 0.0f),
                  float4(0.0f, 0.0f, 1.0f, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, 1.0f));
}

inline matrix zero_matrix() {
    return matrix(float4(0.0f, 0.0f, 0.0f, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, 0.0f));
}

inline matrix perspective_matrix(float fov, float aspect, float near, float far) {
    // Camera points towards -z.  0 < near < far.
    // Matrix maps z range [-near, -far] to [-1, 1], after homogeneous division.
    const float f_h =   1.0f / std::tan(fov * pi / 360.0f);
    const float f_v = aspect / std::tan(fov * pi / 360.0f);
    const float d = 1.0f / (near - far);

    matrix r;
    r[0][0] = f_h;  r[0][1] = 0.0f; r[0][2] = 0.0f;             r[0][3] = 0.0f;
    r[1][0] = 0.0f; r[1][1] = -f_v; r[1][2] = 0.0f;             r[1][3] = 0.0f;
    r[2][0] = 0.0f; r[2][1] = 0.0f; r[2][2] = (near + far) * d; r[2][3] = 2.0f * near * far * d;
    r[3][0] = 0.0f; r[3][1] = 0.0f; r[3][2] = -1.0f;            r[3][3] = 0.0f;

    return r;
}

inline matrix scale_matrix(float x, float y, float z) {
    return matrix(float4(   x, 0.0f, 0.0f, 0.0f),
                  float4(0.0f,    y, 0.0f, 0.0f),
                  float4(0.0f, 0.0f,    z, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, 1.0f));
}

inline matrix translate_matrix(float x, float y, float z) {
    return matrix(float4(1.0f, 0.0f, 0.0f,    x),
                  float4(0.0f, 1.0f, 0.0f,    y),
                  float4(0.0f, 0.0f, 1.0f,    z),
                  float4(0.0f, 0.0f, 0.0f, 1.0f));
}

inline float determinant(const matrix& a) {
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

inline matrix transpose(const matrix& a) {
    return matrix(float4(a.rows[0][0], a.rows[1][0], a.rows[2][0], a.rows[3][0]),
                  float4(a.rows[0][1], a.rows[1][1], a.rows[2][1], a.rows[3][1]),
                  float4(a.rows[0][2], a.rows[1][2], a.rows[2][2], a.rows[3][2]),
                  float4(a.rows[0][3], a.rows[1][3], a.rows[2][3], a.rows[3][3]));
}

inline matrix operator * (const matrix& a, const matrix& b) {
    matrix t = transpose(b);
    return matrix(float4(dot(a[0], t[0]), dot(a[0], t[1]), dot(a[0], t[2]), dot(a[0], t[3])),
                  float4(dot(a[1], t[0]), dot(a[1], t[1]), dot(a[1], t[2]), dot(a[1], t[3])),
                  float4(dot(a[2], t[0]), dot(a[2], t[1]), dot(a[2], t[2]), dot(a[2], t[3])),
                  float4(dot(a[3], t[0]), dot(a[3], t[1]), dot(a[3], t[2]), dot(a[3], t[3])));
}

inline matrix operator * (const matrix& a, float b) {
    return matrix(a.rows[0] * b, a.rows[1] * b, a.rows[2] * b, a.rows[3] * b);
}

inline matrix operator * (float a, const matrix& b) {
    return b * a;
}

inline matrix invert(const matrix& a) {
    matrix result;

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
        return zero_matrix();

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

inline float4 transform(const matrix& a, const float4& b) {
    return float4(dot(a.rows[0], b),
                  dot(a.rows[1], b),
                  dot(a.rows[2], b),
                  dot(a.rows[3], b));
}

inline float3 transform_point(const matrix& a, const float3& b) {
    float4 t = transform(a, float4(b, 1.0f));
    t *= 1.0f / t.w;
    return float3(t.x, t.y, t.z);
}

inline float3 transform_vector(const matrix& a, const float3& b) {
    float4 t = transform(a, float4(b, 0.0f));
    return float3(t.x, t.y, t.z);
}

}

#endif
