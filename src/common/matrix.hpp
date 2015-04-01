#ifndef IMBA_MATRIX_HPP
#define IMBA_MATRIX_HPP

#include "vector.hpp"
#include <ostream>

namespace imba {

/// Column-major 3x3 matrix
struct Mat3 {
    Mat3() {}

    Mat3(float x) {
        m[0] = x;
        m[1] = 0.0f;
        m[2] = 0.0f;

        m[3] = 0.0f;
        m[4] = x;
        m[5] = 0.0f;

        m[6] = 0.0f;
        m[7] = 0.0f;
        m[8] = x;
    }

    Mat3(float m00, float m01, float m02,
         float m10, float m11, float m12,
         float m20, float m21, float m22) {
        m[0] = m00;
        m[1] = m01;
        m[2] = m02;

        m[3] = m10;
        m[4] = m11;
        m[5] = m12;

        m[6] = m20;
        m[7] = m21;
        m[8] = m22;
    }

    float operator [] (int i) const { return m[i]; }
    float& operator [] (int i) { return m[i]; }
     
    float operator () (int i, int j) const { return m[3 * j + i]; }
    float& operator () (int i, int j) { return m[3 * j + i]; }

    static Mat3 identity() {
        return Mat3(1.0f);
    }

    static Mat3 scale(const Vec3& s) {
        return Mat3(s[0], 0.0f, 0.0f,
                    0.0f, s[1], 0.0f,
                    0.0f, 0.0f, s[2]);
    }

    static Mat3 rotation(float angle, const Vec3& axis) {
        const float s = sinf(angle);
        const float c = cosf(angle);
        const float xx = axis[0] * axis[0];
        const float xy = axis[0] * axis[1];
        const float xz = axis[0] * axis[2];
        const float yy = axis[1] * axis[1];
        const float yz = axis[1] * axis[2];
        const float zz = axis[2] * axis[2];
        const float xs = axis[0] * s;
        const float ys = axis[1] * s;
        const float zs = axis[2] * s;

        return Mat3(xx + (1 - xx) * c, xy * (1 - c) + zs, xz * (1 - c) - ys,
                    xy * (1 - c) - zs, yy + (1 - yy) * c, yz * (1 - c) + xs,
                    xz * (1 - c) + ys, yz * (1 - c) - xs, zz + (1 - zz) * c);
    }

    float m[9];
};

inline Mat3 operator + (const Mat3& a, const Mat3& b) {
    return Mat3(a[0] + b[0], a[1] + b[1], a[2] + b[2],
                a[3] + b[3], a[4] + b[4], a[5] + b[5],
                a[6] + b[6], a[7] + b[7], a[8] + b[8]);
}

inline Mat3 operator - (const Mat3& a, const Mat3& b) {
    return Mat3(a[0] - b[0], a[1] - b[1], a[2] - b[2],
                a[3] - b[3], a[4] - b[4], a[5] - b[5],
                a[6] - b[6], a[7] - b[7], a[8] - b[8]);
}

inline Mat3 operator * (const Mat3& a, const Mat3& b) {
    return Mat3(a[0] * b[0] + a[3] * b[1] + a[6] * b[2],
                a[0] * b[3] + a[3] * b[4] + a[6] * b[5],
                a[0] * b[6] + a[3] * b[7] + a[6] * b[8],

                a[1] * b[0] + a[4] * b[1] + a[7] * b[2],
                a[1] * b[3] + a[4] * b[4] + a[7] * b[5],
                a[1] * b[6] + a[4] * b[7] + a[7] * b[8],

                a[2] * b[0] + a[5] * b[3] + a[8] * b[6],
                a[2] * b[1] + a[5] * b[4] + a[8] * b[7],
                a[2] * b[2] + a[5] * b[5] + a[8] * b[8]);
}

inline Vec3 operator * (const Mat3& m, const Vec3& v) {
    return Vec3(m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
                m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
                m[6] * v[0] + m[7] * v[1] + m[8] * v[2]);
}

inline Mat3 operator * (float t, const Mat3& m) {
    return Mat3(t * m[0], t * m[1], t * m[2],
                t * m[3], t * m[4], t * m[5],
                t * m[6], t * m[7], t * m[8]);
}

inline Mat3 operator * (const Mat3& m, float t) {
    return Mat3(t * m[0], t * m[1], t * m[2],
                t * m[3], t * m[4], t * m[5],
                t * m[6], t * m[7], t * m[8]);
}

inline Mat3 transpose(const Mat3& m) {
    return Mat3(m[0], m[3], m[6],
                m[1], m[4], m[7],
                m[2], m[5], m[8]);
}

template <int i, int j>
inline float cofactor(const Mat3& m) {
    constexpr int a = (i + 1) % 3;
    constexpr int b = (i + 2) % 3;
    constexpr int c = (j + 1) % 3;
    constexpr int d = (j + 2) % 3;

    return m(a, c) * m(b, d) - m(b, c) * m(a, d);
}

inline Mat3 inverse(const Mat3& m) {
    Mat3 inv;

    inv(0, 0) = cofactor<0, 0>(m);
    inv(0, 1) = cofactor<1, 0>(m);
    inv(0, 2) = cofactor<2, 0>(m);

    inv(1, 0) = cofactor<0, 1>(m);
    inv(1, 1) = cofactor<1, 1>(m);
    inv(1, 2) = cofactor<2, 1>(m);

    inv(2, 0) = cofactor<0, 2>(m);
    inv(2, 1) = cofactor<1, 2>(m);
    inv(2, 2) = cofactor<2, 2>(m);

    const float det = m(0, 0) * inv(0, 0) + m(0, 1) * inv(1, 0) + m(0, 2) * inv(2, 0);

    if (det == 0)
        return Mat3(0.0f);
    else
        return inv * (1.0f / det); 
}

/// Column major 4x4 matrix
struct Mat4 {
    Mat4() {}

    Mat4(float x) {
        m[0] = x;
        m[1] = 0.0f;
        m[2] = 0.0f;
        m[3] = 0.0f;

        m[4] = 0.0f;
        m[5] = x;
        m[6] = 0.0f;
        m[7] = 0.0f;

        m[8] = 0.0f;
        m[9] = 0.0f;
        m[10] = x;
        m[11] = 0.0f;

        m[12] = 0.0f;
        m[13] = 0.0f;
        m[14] = 0.0f;
        m[15] = x;
    }

    Mat4(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33) {
        m[0] = m00;
        m[1] = m01;
        m[2] = m02;
        m[3] = m03;

        m[4] = m10;
        m[5] = m11;
        m[6] = m12;
        m[7] = m13;

        m[8] = m20;
        m[9] = m21;
        m[10] = m22;
        m[11] = m23;

        m[12] = m30;
        m[13] = m31;
        m[14] = m32;
        m[15] = m33;
    }

    float operator [] (int i) const { return m[i]; }
    float& operator [] (int i) { return m[i]; }
     
    float operator () (int i, int j) const { return m[4 * j + i]; }
    float& operator () (int i, int j) { return m[4 * j + i]; }

    static Mat4 identity() {
        return Mat4(1.0f);
    }

    static Mat4 scale(const Vec3& s) {
        return Mat4(s[0], 0.0f, 0.0f, 0.0f,
                    0.0f, s[1], 0.0f, 0.0f,
                    0.0f, 0.0f, s[2], 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f);
    }

    static Mat4 translation(const Vec3& t) {
        return Mat4(1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    t[0], t[1], t[2], 1.0f);
    }

    static Mat4 rotation(float angle, const Vec3& axis) {
        const float s = sinf(angle);
        const float c = cosf(angle);
        const float xx = axis[0] * axis[0];
        const float xy = axis[0] * axis[1];
        const float xz = axis[0] * axis[2];
        const float yy = axis[1] * axis[1];
        const float yz = axis[1] * axis[2];
        const float zz = axis[2] * axis[2];
        const float xs = axis[0] * s;
        const float ys = axis[1] * s;
        const float zs = axis[2] * s;

        return Mat4(xx + (1 - xx) * c, xy * (1 - c) + zs, xz * (1 - c) - ys, 0.0f,
                    xy * (1 - c) - zs, yy + (1 - yy) * c, yz * (1 - c) + xs, 0.0f,
                    xz * (1 - c) + ys, yz * (1 - c) - xs, zz + (1 - zz) * c, 0.0f,
                                 0.0f,              0.0f,              0.0f, 1.0f);
    }

    float m[16];
};

inline Mat4 operator + (const Mat4& a, const Mat4& b) {
    return Mat4(a[ 0] + b[ 0], a[ 1] + b[ 1], a[ 2] + b[ 2], a[ 3] + b[ 3],
                a[ 4] + b[ 4], a[ 5] + b[ 5], a[ 6] + b[ 6], a[ 7] + b[ 7],
                a[ 8] + b[ 8], a[ 9] + b[ 9], a[10] + b[10], a[11] + b[11],
                a[12] + b[12], a[13] + b[13], a[14] + b[14], a[15] + b[15]);
}

inline Mat4 operator - (const Mat4& a, const Mat4& b) {
    return Mat4(a[ 0] - b[ 0], a[ 1] - b[ 1], a[ 2] - b[ 2], a[ 3] - b[ 3],
                a[ 4] - b[ 4], a[ 5] - b[ 5], a[ 6] - b[ 6], a[ 7] - b[ 7],
                a[ 8] - b[ 8], a[ 9] - b[ 9], a[10] - b[10], a[11] - b[11],
                a[12] - b[12], a[13] - b[13], a[14] - b[14], a[15] - b[15]);
}

inline Mat4 operator * (const Mat4& a, const Mat4& b) {
    return Mat4(a[0] * b[0] + a[4] * b[1] + a[ 8] * b[2] + a[12] * b[3],
                a[1] * b[0] + a[5] * b[1] + a[ 9] * b[2] + a[13] * b[3],
                a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3],
                a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3],

                a[0] * b[4] + a[4] * b[5] + a[ 8] * b[6] + a[12] * b[7],
                a[1] * b[4] + a[5] * b[5] + a[ 9] * b[6] + a[13] * b[7],
                a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7],
                a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7],

                a[0] * b[8] + a[4] * b[9] + a[ 8] * b[10] + a[12] * b[11],
                a[1] * b[8] + a[5] * b[9] + a[ 9] * b[10] + a[13] * b[11],
                a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11],
                a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11],

                a[0] * b[12] + a[4] * b[13] + a[ 8] * b[14] + a[12] * b[15],
                a[1] * b[12] + a[5] * b[13] + a[ 9] * b[14] + a[13] * b[15],
                a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15],
                a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15]);
}

inline Vec4 operator * (const Mat4& m, const Vec4& v) {
    return Vec4(m[0] * v[0] + m[4] * v[1] + m[ 8] * v[2] + m[12] * v[3],
                m[1] * v[0] + m[5] * v[1] + m[ 9] * v[2] + m[13] * v[3],
                m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3],
                m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3]);
}

inline Vec3 operator * (const Mat4& m, const Vec3& v) {
    return Vec3(m[0] * v[0] + m[4] * v[1] + m[ 8] * v[2] + m[12],
                m[1] * v[0] + m[5] * v[1] + m[ 9] * v[2] + m[13],
                m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14]);
}

inline Mat4 operator * (float t, const Mat4& m) {
    return Mat4(t * m[ 0], t * m[ 1], t * m[ 2], t * m[ 3],
                t * m[ 4], t * m[ 5], t * m[ 6], t * m[ 7],
                t * m[ 8], t * m[ 9], t * m[10], t * m[11],
                t * m[12], t * m[13], t * m[14], t * m[15]);
}

inline Mat4 operator * (const Mat4& m, float t) {
    return Mat4(t * m[ 0], t * m[ 1], t * m[ 2], t * m[ 3],
                t * m[ 4], t * m[ 5], t * m[ 6], t * m[ 7],
                t * m[ 8], t * m[ 9], t * m[10], t * m[11],
                t * m[12], t * m[13], t * m[14], t * m[15]);
}


inline Mat4 transpose(const Mat4& m) {
    return Mat4(m[0], m[4], m[ 8], m[12],
                m[1], m[5], m[ 9], m[13],
                m[2], m[6], m[10], m[14],
                m[3], m[7], m[11], m[15]);
}

template <int i, int j>
inline float cofactor(const Mat4& m) {
    constexpr int s = ((i + j) % 2) ? -1 : 1;
    constexpr int a = (i + 1) % 4;
    constexpr int b = (i + 2) % 4;
    constexpr int c = (i + 3) % 4;

    constexpr int d = (j + 1) % 4;
    constexpr int e = (j + 2) % 4;
    constexpr int f = (j + 3) % 4;

    return s * (m(a, d) * (m(b, e) * m(c, f) - m(c, e) * m(b, f)) -
                m(a, e) * (m(b, d) * m(c, f) - m(c, d) * m(b, f)) +
                m(a, f) * (m(b, d) * m(c, e) - m(c, d) * m(b, e)));
}

inline Mat4 inverse(const Mat4& m) {
    Mat4 inv;

    inv(0, 0) = cofactor<0, 0>(m);
    inv(0, 1) = cofactor<1, 0>(m);
    inv(0, 2) = cofactor<2, 0>(m);
    inv(0, 3) = cofactor<3, 0>(m);

    inv(1, 0) = cofactor<0, 1>(m);
    inv(1, 1) = cofactor<1, 1>(m);
    inv(1, 2) = cofactor<2, 1>(m);
    inv(1, 3) = cofactor<3, 1>(m);

    inv(2, 0) = cofactor<0, 2>(m);
    inv(2, 1) = cofactor<1, 2>(m);
    inv(2, 2) = cofactor<2, 2>(m);
    inv(2, 3) = cofactor<3, 2>(m);

    inv(3, 0) = cofactor<0, 3>(m);
    inv(3, 1) = cofactor<1, 3>(m);
    inv(3, 2) = cofactor<2, 3>(m);
    inv(3, 3) = cofactor<3, 3>(m);

    const float det = m(0, 0) * inv(0, 0) + m(0, 1) * inv(1, 0) + m(0, 2) * inv(2, 0) + m(0, 3) * inv(3, 0);

    if (det == 0)
        return Mat4(0.0f);
    else
        return inv * (1.0f / det); 
}

inline std::ostream& operator << (std::ostream& os, const Mat4& m) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            os << m(i, j) << " ";
        }
        os << std::endl;
    }
    return os;
}


inline std::ostream& operator << (std::ostream& os, const Mat3& m) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            os << m(i, j) << " ";
        }
        os << std::endl;
    }
    return os;
}

} // namespace imba

#endif // IMBA_MATRIX_HPP

