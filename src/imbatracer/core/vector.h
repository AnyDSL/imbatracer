#ifndef IMBA_VECTOR_H
#define IMBA_VECTOR_H

#include "expr.h"

namespace imba {

/// Utility class to set the contents of a vector.
template <int I, int J, int K = 0>
struct SetVector {
    template <typename T, typename E, int N, int M>
    static void set(Vector<T, N>& v, const Expr<T, M, E>& e) {
        static_assert(I < N && K < M, "Too many initializers for vector");
        v.values[I] = e[K];
        SetVector<I + 1, J, K + 1>::set(v, e);
    }
};

template <int J, int K>
struct SetVector<J, J, K> {
    template <typename T, typename E, int N, int M>
    static void set(Vector<T, N>& v, const Expr<T, M, E>& e) {}
};

/// Base class that defines the contents of a vector.
template <typename T, int N>
struct VectorElements {
    T values[N];
};

template <typename T>
struct VectorElements<T, 4> {
    union {
        struct { T x, y, z, w; };
        T values[4];
    };
};

template <typename T>
struct VectorElements<T, 3> {
    union {
        struct { T x, y, z; };
        T values[3];
    };
};

template <typename T>
struct VectorElements<T, 2> {
    union {
        struct { T x, y; };
        T values[2];
    };
};

/// A class that represents a vector whose elements are stored.
template <typename T, int N>
struct Vector : public Expr<T, N, Vector<T, N> >, public VectorElements<T, N> {
    T operator [] (int i) const { return VectorElements<T, N>::values[i]; }
    T& operator [] (int i) { return VectorElements<T, N>::values[i]; }

    // These constructors are implicit because they are safe
    Vector() {}
    template <typename E>
    Vector(const Expr<T, N, E>& e) { set(e); }

    // This constructor has to be explicit since it may truncate a bigger vector
    template <typename... Args>
    explicit Vector(Args... args) { set(args...); }
    explicit Vector(T t) { set(ConstantExpr<T, N>(t)); }

    template <typename E>
    Vector& operator += (const Expr<T, N, E>& e) {
        return (*this = *this + e);
    }

    template <typename E>
    Vector&  operator -= (const Expr<T, N, E>& e) {
        return (*this = *this - e);
    }

    template <typename E>
    Vector&  operator *= (const Expr<T, N, E>& e) {
        return (*this = *this * e);
    }

    template <typename E>
    Vector&  operator /= (const Expr<T, N, E>& e) {
        return (*this = *this / e);
    }

    Vector&  operator *= (const ConstantExpr<T, N>& e) {
        return (*this = *this * e);
    }

    Vector&  operator /= (const ConstantExpr<T, N>& e) {
        return (*this = *this / e);
    }

    template <int I = 0, typename... Args>
    void set(T x, Args... args) {
        // This will warn if the number of *scalar* initializers is greater than N
        static_assert(I < N, "Too many initializers for vector");
        VectorElements<T, N>::values[I] = x;
        set<I + 1>(args...);
    }

    template <int I = 0, typename E, int M, typename... Args>
    void set(const Expr<T, M, E>& e, Args... args) {
        SetVector<min(I, N), min(I + M, N), 0>::set(*this, e);
        set<I + M>(args...);
    }

    template <int I = 0>
    void set() {}

    static ConstantExpr<T, N> zero() { return ConstantExpr<T, N>(T(0)); }
    static ConstantExpr<T, N> one() { return ConstantExpr<T, N>(T(1)); }

private:
    // Until C++14, std::min is not constexpr
    static constexpr int min(int A, int B) { return A < B ? A : B; }
};

template <typename T, typename A, typename B>
Vector<T, 3> cross(const Expr<T, 3, A>& a, const Expr<T, 3, B>& b) {
    return Vector<T, 3>(a[1] * b[2] - a[2] * b[1],
                            a[2] * b[0] - a[0] * b[2],
                            a[0] * b[1] - a[1] * b[0]);
}

template <typename T>
Vector<T, 3> rotate(const Vector<T, 3>& v, const Vector<T, 3>& axis, T angle) {
    T q[4];
    q[0] = axis.x * sinf(angle / 2);
    q[1] = axis.y * sinf(angle / 2);
    q[2] = axis.z * sinf(angle / 2);
    q[3] = cosf(angle / 2);

    T p[4];
    p[0] = q[3] * v.x + q[1] * v.z - q[2] * v.y;
    p[1] = q[3] * v.y - q[0] * v.z + q[2] * v.x;
    p[2] = q[3] * v.z + q[0] * v.y - q[1] * v.x;
    p[3] = -(q[0] * v.x + q[1] * v.y + q[2] * v.z);

    return Vector<T, 3>(p[3] * -q[0] + p[0] * q[3] + p[1] * -q[2] - p[2] * -q[1],
                            p[3] * -q[1] - p[0] * -q[2] + p[1] * q[3] + p[2] * -q[0],
                            p[3] * -q[2] + p[0] * -q[1] - p[1] * -q[0] + p[2] * q[3]);
}

typedef Vector<float, 2> float2;
typedef Vector<float, 3> float3;
typedef Vector<float, 4> float4;

} // namespace imba

#endif // IMBA_VECTOR_H
