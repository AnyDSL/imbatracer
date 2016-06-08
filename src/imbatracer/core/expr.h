#ifndef IMBA_EXPR_H
#define IMBA_EXPR_H

#include "common.h"
#include <cmath>

namespace imba {

template <typename T, int N>
struct Vector;

template <typename T, int N>
struct ConstantExpr;

template <typename T, int N, typename Op, typename E>
struct UnaryExpr;

template <typename T, int N, typename Op, typename A, typename B>
struct BinaryExpr;

template <typename T, int N, typename Op, typename A, typename B, typename C>
struct TernaryExpr;

template <typename T>
struct ExprRef { typedef T Type; };

template <typename T, int N>
struct ExprRef<Vector<T, N> > { typedef const Vector<T, N>& Type; };

namespace op {
    template <typename T> struct Abs { T operator () (T a) { return std::abs(a); } };
    template <typename T> struct Neg { T operator () (T a) { return -a; } };
    template <typename T> struct Rcp { T operator () (T a) { return T(1) / a; } };

    template <typename T> struct Add { T operator () (T a, T b) { return a + b; } };
    template <typename T> struct Sub { T operator () (T a, T b) { return a - b; } };
    template <typename T> struct Mul { T operator () (T a, T b) { return a * b; } };
    template <typename T> struct Div { T operator () (T a, T b) { return a / b; } };
    template <typename T> struct Min { T operator () (T a, T b) { return std::min(a, b); } };
    template <typename T> struct Max { T operator () (T a, T b) { return std::max(a, b); } };

    template <typename T> struct Clamp { T operator () (T a, T b, T c) { return std::max(b, std::min(c, a)); } };
}

/// Base class for expressions, with the "expression templates" pattern.
template <typename T, int N, typename E>
struct Expr {
    T operator [] (int i) const { return static_cast<const E*>(this)->operator [] (i); }

    friend BinaryExpr<T, N, op::Mul<T>, ConstantExpr<T, N>, E> operator * (const ConstantExpr<T, N>& a, const Expr& b) {
        return BinaryExpr<T, N, op::Mul<T>, ConstantExpr<T, N>, E>(a, b);
    }

    friend BinaryExpr<T, N, op::Mul<T>, ConstantExpr<T, N>, E> operator * (const Expr& a, const ConstantExpr<T, N>& b) {
        return BinaryExpr<T, N, op::Mul<T>, ConstantExpr<T, N>, E>(b, a);
    }

    friend BinaryExpr<T, N, op::Div<T>, ConstantExpr<T, N>, E> operator / (const ConstantExpr<T, N>& a, const Expr& b) {
        return BinaryExpr<T, N, op::Div<T>, ConstantExpr<T, N>, E>(a, b);
    }

    friend BinaryExpr<T, N, op::Mul<T>, E, ConstantExpr<T, N> > operator / (const Expr& a, const ConstantExpr<T, N>& b) {
        return BinaryExpr<T, N, op::Mul<T>, E, ConstantExpr<T, N> >(a, ConstantExpr<T, N>(rcp(b)));
    }
};

/// A class that represents a constant (i.e. uniform) vector.
template <typename T, int N>
struct ConstantExpr : public Expr<T, N, ConstantExpr<T, N> > {
    ConstantExpr(T t) : t(t) {}
    T operator [] (int i) const { return t; }
    T t;
};

/// A class that represents a unary operation.
template <typename T, int N, typename Op, typename E>
struct UnaryExpr : public Expr<T, N, UnaryExpr<T, N, Op, E> > {
    UnaryExpr(const Expr<T, N, E>& e)
        : expr(static_cast<const E&>(e))
    {}

    T operator [] (int i) const {
        Op op;
        return op(expr[i]);
    }

    typename ExprRef<E>::Type expr;
};

/// A class that represents a binary operation.
template <typename T, int N, typename Op, typename A, typename B>
struct BinaryExpr : public Expr<T, N, BinaryExpr<T, N, Op, A, B> > {
    BinaryExpr(const Expr<T, N, A>& l, const Expr<T, N, B>& r)
        : left(static_cast<const A&>(l)), right(static_cast<const B&>(r))
    {}

    T operator [] (int i) const {
        Op op;
        return op(left[i], right[i]);
    }

    typename ExprRef<A>::Type left;
    typename ExprRef<B>::Type right;
};

/// A class that represents a ternary operation.
template <typename T, int N, typename Op, typename A, typename B, typename C>
struct TernaryExpr : public Expr<T, N, TernaryExpr<T, N, Op, A, B, C> > {
    TernaryExpr(const Expr<T, N, A>& f, const Expr<T, N, B>& s, const Expr<T, N, C>& t)
        : first(static_cast<const A&>(f))
        , second(static_cast<const B&>(s))
        , third(static_cast<const C&>(t))
    {}

    T operator [] (int i) const {
        Op op;
        return op(first[i], second[i], third[i]);
    }

    typename ExprRef<A>::Type first;
    typename ExprRef<B>::Type second;
    typename ExprRef<C>::Type third;
};

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

// Binary operators on vectors
template <typename T, int N, typename A, typename B>
BinaryExpr<T, N, op::Add<T>, A, B> operator + (const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    return BinaryExpr<T, N, op::Add<T>, A, B>(a, b);
}

template <typename T, int N, typename A, typename B>
BinaryExpr<T, N, op::Sub<T>, A, B> operator - (const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    return BinaryExpr<T, N, op::Sub<T>, A, B>(a, b);
}

template <typename T, int N, typename A, typename B>
BinaryExpr<T, N, op::Mul<T>, A, B> operator * (const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    return BinaryExpr<T, N, op::Mul<T>, A, B>(a, b);
}

template <typename T, int N, typename A, typename B>
BinaryExpr<T, N, op::Div<T>, A, B> operator / (const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    return BinaryExpr<T, N, op::Div<T>, A, B>(a, b);
}

template <typename T, int N, typename A, typename B>
BinaryExpr<T, N, op::Min<T>, A, B> min(const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    return BinaryExpr<T, N, op::Min<T>, A, B>(a, b);
}

template <typename T, int N, typename A, typename B>
BinaryExpr<T, N, op::Max<T>, A, B> max(const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    return BinaryExpr<T, N, op::Max<T>, A, B>(a, b);
}

// Unary operators
template <typename T, int N, typename E>
UnaryExpr<T, N, op::Abs<T>, E> abs(const Expr<T, N, E>& e) {
    return UnaryExpr<T, N, op::Abs<T>, E>(e);
}

template <typename T, int N, typename E>
UnaryExpr<T, N, op::Neg<T>, E> operator - (const Expr<T, N, E>& e) {
    return UnaryExpr<T, N, op::Neg<T>, E>(e);
}

template <typename T, int N, typename E>
UnaryExpr<T, N, op::Rcp<T>, E> rcp(const Expr<T, N, E>& e) {
    return UnaryExpr<T, N, op::Rcp<T>, E>(e);
}

// Binary operators on scalars
template <typename T, int N>
ConstantExpr<T, N> operator * (const ConstantExpr<T, N>& a, const ConstantExpr<T, N>& b) {
    return ConstantExpr<T, N>(a.t * b.t);
}

template <typename T, int N>
ConstantExpr<T, N> operator / (const ConstantExpr<T, N>& a, const ConstantExpr<T, N>& b) {
    return ConstantExpr<T, N>(a.t / b.t);
}

// Misc.
template <typename T, int N, typename A, typename B, typename C>
TernaryExpr<T, N, op::Clamp<T>, A, B, C> clamp(const Expr<T, N, A>& a, const Expr<T, N, B>& b, const Expr<T, N, C>& c) {
    return TernaryExpr<T, N, op::Clamp<T>, A, B, C>(a, b, c);
}

template <int I, int N>
struct Dot {
    template <typename T, typename A, typename B>
    T operator () (const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
        Dot<I + 1, N> dot;
        return a[I] * b[I] + dot(a, b);
    }
};

template <int N>
struct Dot<N, N> {
    template <typename T, typename A, typename B>
    T operator () (const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
        return T(0);
    }
};

template <typename T, int N, typename A, typename B>
T dot(const Expr<T, N, A>& a, const Expr<T, N, B>& b) {
    Dot<0, N> d;
    return d(a, b);
}

template <typename T, int N, typename E>
T lensqr(const Expr<T, N, E>& e) {
    return dot(e, e);
}

template <typename T, int N, typename E>
T length(const Expr<T, N, E>& e) {
    return std::sqrt(lensqr(e));
}

template <typename T, int N, typename E>
auto normalize(const Expr<T, N, E>& e) -> decltype(e / length(e)) {
    return e / length(e);
}

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

#endif // IMBA_EXPR_H
