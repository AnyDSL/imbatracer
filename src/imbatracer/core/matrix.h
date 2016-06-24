#ifndef IMBA_MATRIX
#define IMBA_MATRIX

#include "vector.h"

namespace imba {

template <typename T, int N, int M>
struct Matrix {
    Vector<Vector<T, M>, N> rows;

    Matrix() {}
    template <typename... Args>
    Matrix(const Args&... args) : rows{args...} {}

    Vector<T, M> operator [] (int i) const { return rows[i]; }
    Vector<T, M>& operator [] (int i) { return rows[i]; }

    static Matrix<T, N, M> constant(T t) {
        Matrix<T, N, M> m;
        #pragma unroll
        for (int i = 0; i < N; i++) {
            #pragma unroll
            for (int j = 0; j < M; j++) {
                m[i][j] = t;
            }
        }
        return m;
    }

    static Matrix<T, N, M> identity() {
        Matrix<T, N, M> m;
        #pragma unroll
        for (int i = 0; i < N; i++) {
            #pragma unroll
            for (int j = 0; j < M; j++) {
                m[i][j] = i == j ? 1 : 0;
            }
        }
        return m;
    }

    static Matrix<T, N, M> zero() { return constant(0); }
    static Matrix<T, N, M> one()  { return constant(1); }
};

template <typename T>
Matrix<T, 4, 4> scale(T x, T y, T z, T w = 1) {
    return Matrix<T, 4, 4>(Vector<T, 4>(x, 0, 0, 0),
                           Vector<T, 4>(0, y, 0, 0),
                           Vector<T, 4>(0, 0, z, 0),
                           Vector<T, 4>(0, 0, 0, w));
}

template <typename T>
Matrix<T, 4, 4> translate(T x, T y, T z) {
    return Matrix<T, 4, 4>(Vector<T, 4>(1, 0, 0, x),
                           Vector<T, 4>(0, 1, 0, y),
                           Vector<T, 4>(0, 0, 1, z),
                           Vector<T, 4>(0, 0, 0, 1));
}

template <typename T>
Matrix<T, 4, 4> perspective(T fov, T aspect, T near, T far) {
    // Camera points towards -z.  0 < near < far.
    // Matrix maps z range [-near, -far] to [-1, 1], after homogeneous division.
    const T f_h =   T(1) / std::tan(fov * pi / T(360));
    const T f_v = aspect / std::tan(fov * pi / T(360));
    const T d   = T(1) / (near - far);

    const T p1 = (near + far) * d;
    const T p2 = 2 * near * far * d;
    return Matrix<T, 4, 4>(Vector<T, 4>(f_h,    0,  0,  0),
                           Vector<T, 4>(0,   -f_v,  0,  0),
                           Vector<T, 4>(0,      0, p1, p2),
                           Vector<T, 4>(0,      0, -1,  0));
}

template <typename T>
Matrix<T, 4, 4> rotate_x(T angle) {
    return Matrix<T, 4, 4>(float4(1,            0,           0, 0),
                           float4(0,  cosf(angle), sinf(angle), 0),
                           float4(0, -sinf(angle), cosf(angle), 0),
                           float4(0,            0,           0, 1));
}

template <typename T>
Matrix<T, 4, 4> rotate_y(T angle) {
    return Matrix<T, 4, 4>(float4(cosf(angle), 0, -sinf(angle), 0),
                           float4(          0, 1,            0, 0),
                           float4(sinf(angle), 0,  cosf(angle), 0),
                           float4(          0, 0,            0, 1));
}

template <typename T>
Matrix<T, 4, 4> rotate_z(T angle) {
    return Matrix<T, 4, 4>(float4( cosf(angle), sinf(angle), 0, 0),
                           float4(-sinf(angle), cosf(angle), 0, 0),
                           float4(           0,           0, 1, 0),
                           float4(           0,           0, 0, 1));
}

template <typename T>
T determinant(const Matrix<T, 4, 4>& a) {
    const T m0 =  a[1][1] * a[2][2] * a[3][3] - a[1][1] * a[2][3] * a[3][2] - a[2][1] * a[1][2] * a[3][3]
                 +a[2][1] * a[1][3] * a[3][2] + a[3][1] * a[1][2] * a[2][3] - a[3][1] * a[1][3] * a[2][2];
    const T m1 = -a[1][0] * a[2][2] * a[3][3] + a[1][0] * a[2][3] * a[3][2] + a[2][0] * a[1][2] * a[3][3]
                 -a[2][0] * a[1][3] * a[3][2] - a[3][0] * a[1][2] * a[2][3] + a[3][0] * a[1][3] * a[2][2];
    const T m2 =  a[1][0] * a[2][1] * a[3][3] - a[1][0] * a[2][3] * a[3][1] - a[2][0] * a[1][1] * a[3][3]
                 +a[2][0] * a[1][3] * a[3][1] + a[3][0] * a[1][1] * a[2][3] - a[3][0] * a[1][3] * a[2][1];
    const T m3 = -a[1][0] * a[2][1] * a[3][2] + a[1][0] * a[2][2] * a[3][1] + a[2][0] * a[1][1] * a[3][2]
                 -a[2][0] * a[1][2] * a[3][1] - a[3][0] * a[1][1] * a[2][2] + a[3][0] * a[1][2] * a[2][1];
    return a[0][0] * m0 + a[0][1] * m1 + a[0][2] * m2 + a[0][3] * m3;
}

template <typename T, int N, int M>
Matrix<T, M, N> transpose(const Matrix<T, N, M>& a) {
    Matrix<T, M, N> m;
    #pragma unroll
    for (int i = 0; i < N; i++) {
        #pragma unroll
        for (int j = 0; j < M; j++) {
            m[j][i] = a[i][j];
        }
    }
    return m;
}

template <typename T>
Matrix<T, 4, 4> invert(const Matrix<T, 4, 4>& a) {
    Matrix<T, 4, 4> result;

    //Taken from http://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    result[0][0] = +a[1][1] * a[2][2] * a[3][3] - a[1][1] * a[2][3] * a[3][2] - a[2][1] * a[1][2] * a[3][3]
                   +a[2][1] * a[1][3] * a[3][2] + a[3][1] * a[1][2] * a[2][3] - a[3][1] * a[1][3] * a[2][2];
    result[1][0] = -a[1][0] * a[2][2] * a[3][3] + a[1][0] * a[2][3] * a[3][2] + a[2][0] * a[1][2] * a[3][3]
                   -a[2][0] * a[1][3] * a[3][2] - a[3][0] * a[1][2] * a[2][3] + a[3][0] * a[1][3] * a[2][2];
    result[2][0] = +a[1][0] * a[2][1] * a[3][3] - a[1][0] * a[2][3] * a[3][1] - a[2][0] * a[1][1] * a[3][3]
                   +a[2][0] * a[1][3] * a[3][1] + a[3][0] * a[1][1] * a[2][3] - a[3][0] * a[1][3] * a[2][1];
    result[3][0] = -a[1][0] * a[2][1] * a[3][2] + a[1][0] * a[2][2] * a[3][1] + a[2][0] * a[1][1] * a[3][2]
                   -a[2][0] * a[1][2] * a[3][1] - a[3][0] * a[1][1] * a[2][2] + a[3][0] * a[1][2] * a[2][1];

    const T det = a[0][0] * result[0][0] + a[0][1] * result[1][0] + a[0][2] * result[2][0] + a[0][3] * result[3][0];
    if (det == 0)
        return Matrix<T, 4, 4>::zero();

    result[0][1] = -a[0][1] * a[2][2] * a[3][3] + a[0][1] * a[2][3] * a[3][2] + a[2][1] * a[0][2] * a[3][3]
                   -a[2][1] * a[0][3] * a[3][2] - a[3][1] * a[0][2] * a[2][3] + a[3][1] * a[0][3] * a[2][2];
    result[1][1] = +a[0][0] * a[2][2] * a[3][3] - a[0][0] * a[2][3] * a[3][2] - a[2][0] * a[0][2] * a[3][3]
                   +a[2][0] * a[0][3] * a[3][2] + a[3][0] * a[0][2] * a[2][3] - a[3][0] * a[0][3] * a[2][2];
    result[2][1] = -a[0][0] * a[2][1] * a[3][3] + a[0][0] * a[2][3] * a[3][1] + a[2][0] * a[0][1] * a[3][3]
                   -a[2][0] * a[0][3] * a[3][1] - a[3][0] * a[0][1] * a[2][3] + a[3][0] * a[0][3] * a[2][1];
    result[3][1] = +a[0][0] * a[2][1] * a[3][2] - a[0][0] * a[2][2] * a[3][1] - a[2][0] * a[0][1] * a[3][2]
                   +a[2][0] * a[0][2] * a[3][1] + a[3][0] * a[0][1] * a[2][2] - a[3][0] * a[0][2] * a[2][1];
    result[0][2] = +a[0][1] * a[1][2] * a[3][3] - a[0][1] * a[1][3] * a[3][2] - a[1][1] * a[0][2] * a[3][3]
                   +a[1][1] * a[0][3] * a[3][2] + a[3][1] * a[0][2] * a[1][3] - a[3][1] * a[0][3] * a[1][2];
    result[1][2] = -a[0][0] * a[1][2] * a[3][3] + a[0][0] * a[1][3] * a[3][2] + a[1][0] * a[0][2] * a[3][3]
                   -a[1][0] * a[0][3] * a[3][2] - a[3][0] * a[0][2] * a[1][3] + a[3][0] * a[0][3] * a[1][2];
    result[2][2] = +a[0][0] * a[1][1] * a[3][3] - a[0][0] * a[1][3] * a[3][1] - a[1][0] * a[0][1] * a[3][3]
                   +a[1][0] * a[0][3] * a[3][1] + a[3][0] * a[0][1] * a[1][3] - a[3][0] * a[0][3] * a[1][1];
    result[3][2] = -a[0][0] * a[1][1] * a[3][2] + a[0][0] * a[1][2] * a[3][1] + a[1][0] * a[0][1] * a[3][2]
                   -a[1][0] * a[0][2] * a[3][1] - a[3][0] * a[0][1] * a[1][2] + a[3][0] * a[0][2] * a[1][1];
    result[0][3] = -a[0][1] * a[1][2] * a[2][3] + a[0][1] * a[1][3] * a[2][2] + a[1][1] * a[0][2] * a[2][3]
                   -a[1][1] * a[0][3] * a[2][2] - a[2][1] * a[0][2] * a[1][3] + a[2][1] * a[0][3] * a[1][2];
    result[1][3] = +a[0][0] * a[1][2] * a[2][3] - a[0][0] * a[1][3] * a[2][2] - a[1][0] * a[0][2] * a[2][3]
                   +a[1][0] * a[0][3] * a[2][2] + a[2][0] * a[0][2] * a[1][3] - a[2][0] * a[0][3] * a[1][2];
    result[2][3] = -a[0][0] * a[1][1] * a[2][3] + a[0][0] * a[1][3] * a[2][1] + a[1][0] * a[0][1] * a[2][3]
                   -a[1][0] * a[0][3] * a[2][1] - a[2][0] * a[0][1] * a[1][3] + a[2][0] * a[0][3] * a[1][1];
    result[3][3] = +a[0][0] * a[1][1] * a[2][2] - a[0][0] * a[1][2] * a[2][1] - a[1][0] * a[0][1] * a[2][2]
                   +a[1][0] * a[0][2] * a[2][1] + a[2][0] * a[0][1] * a[1][2] - a[2][0] * a[0][2] * a[1][1];

    return result * (1.0f / det);
}

template <typename T, int N, int M, int P>
Matrix<T, N, P> operator * (const Matrix<T, N, M>& a, const Matrix<T, M, P>& b) {
    Matrix<T, N, P> p;
    #pragma unroll
    for (int i = 0; i < N; i++) {
        #pragma unroll
        for (int j = 0; j < P; j++) {
            T d(0);
            #pragma unroll
            for (int k = 0; k < M; k++) d += a[i][k] * b[k][j];
            p[i][j] = d;
        }
    }
    return p;
}

template <typename T, int N, int M>
Matrix<T, N, M> operator * (const Matrix<T, N, M>& a, T b) {
    Matrix<T, N, M> p;
    #pragma unroll
    for (int i = 0; i < N; i++) p[i] = a[i] * b;
    return p;
}

template <typename T, int N, int M>
Matrix<T, N, M> operator * (T a, const Matrix<T, N, M>& b) {
    return b * a;
}

template <typename T, int N, int M, typename E>
Vector<T, N> operator * (const Matrix<T, M, N>& a, const Expr<T, N, E>& b) {
    Vector<T, N> v;
    #pragma unroll
    for (int i = 0; i < N; i++) v[i] = dot(a[i], b);
    return v;
}

template <typename T, typename E>
Vector<T, 3> project(const Matrix<T, 4, 4>& a, const Expr<T, 3, E>& b) {
    const auto t = a * Vector<T, 4>(b, 1);
    return Vector<T, 3>(t.x, t.y, t.z) / t.w;
}

template <typename T, int N, int M>
Matrix<T, N, M> abs(const Matrix<T, N, M>& a) {
    Matrix<T, N, M> res;
    #pragma unroll
    for (int i = 0; i < N; i++) res[i] = abs(a[i]);
    return res;
}

template <typename T>
Matrix<T, 4, 4> euler(T x, T y, T z) {
    return rotate_x(x) * rotate_y(y) * rotate_z(z);
}

typedef Matrix<float, 3, 3> float3x3;
typedef Matrix<float, 3, 4> float3x4;
typedef Matrix<float, 4, 4> float4x4;

} // namespace imba

#endif // IMBA_MATRIX_H
