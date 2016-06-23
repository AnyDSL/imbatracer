#ifndef IMBA_EXPR_H
#define IMBA_EXPR_H

#include <cmath>

#include "common.h"

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

} // namespace imba

#endif // IMBA_EXPR_H
