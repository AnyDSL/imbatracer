#ifndef IMBA_FLOAT4X4
#define IMBA_FLOAT4X4

#include "float3.h"

namespace imba {

struct float4x4 {
    float4 rows[4];

    float4x4() {}
    float4x4(const float4& r0, const float4& r1, const float4& r2, const float4& r3) : rows{r0, r1, r2, r3} {}

    const float4& operator [] (int row) const { return rows[row]; }
    float4& operator [] (int row) { return rows[row]; }

    float4 row(int i) const { return rows[i]; }
    float4 col(int i) const { return float4(rows[0][i], rows[1][i], rows[2][i], rows[3][i]); }

    static float4x4 identity() {
        return float4x4(float4(1.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 1.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 1.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static float4x4 zero() {
        return float4x4(float4(0.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    static float4x4 perspective(float fov, float aspect, float near, float far) {
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

    static float4x4 scaling(float x, float y, float z) {
        return float4x4(float4(   x, 0.0f, 0.0f, 0.0f),
                        float4(0.0f,    y, 0.0f, 0.0f),
                        float4(0.0f, 0.0f,    z, 0.0f),
                        float4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static float4x4 translation(float x, float y, float z) {
        return float4x4(float4(1.0f, 0.0f, 0.0f,    x),
                        float4(0.0f, 1.0f, 0.0f,    y),
                        float4(0.0f, 0.0f, 1.0f,    z),
                        float4(0.0f, 0.0f, 0.0f, 1.0f));
    }
};

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
    return float4x4(a.col(0), a.col(1), a.col(2), a.col(3));
}

inline float4x4 operator * (const float4x4& a, const float4x4& b) {
    return float4x4(float4(dot(a[0], b.col(0)), dot(a[0], b.col(1)), dot(a[0], b.col(2)), dot(a[0], b.col(3))),
                    float4(dot(a[1], b.col(0)), dot(a[1], b.col(1)), dot(a[1], b.col(2)), dot(a[1], b.col(3))),
                    float4(dot(a[2], b.col(0)), dot(a[2], b.col(1)), dot(a[2], b.col(2)), dot(a[2], b.col(3))),
                    float4(dot(a[3], b.col(0)), dot(a[3], b.col(1)), dot(a[3], b.col(2)), dot(a[3], b.col(3))));
}

inline float4 operator * (const float4x4& a, const float4& b) {
    return float4(dot(a[0], b),
                  dot(a[1], b),
                  dot(a[2], b),
                  dot(a[3], b));
}

inline float4 operator * (const float4& a, const float4x4& b) {
    return float4(dot(a, b.col(0)),
                  dot(a, b.col(1)),
                  dot(a, b.col(2)),
                  dot(a, b.col(3)));
}

inline float4x4 operator * (const float4x4& a, float b) {
    return float4x4(a[0] * b, a[1] * b, a[2] * b, a[3] * b);
}

inline float4x4 operator * (float a, const float4x4& b) {
    return b * a;
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

inline float3 transform_point(const float4x4& a, const float3& b) {
    const float4 t = a * float4(b, 1.0f);
    return float3(t.x, t.y, t.z) / t.w;
}

inline float3 transform_vector(const float4x4& a, const float3& b) {
    const float4 t = a * float4(b, 0.0f);
    return float3(t.x, t.y, t.z);
}

}

#endif
