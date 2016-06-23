#ifndef IMBA_ATOMIC_VECTOR_H
#define IMBA_ATOMIC_VECTOR_H

#include "expr.h"
#include <atomic>

namespace imba {

template<typename T, int N, typename V>
struct AtomicVector : public Expr<T, N, AtomicVector<T, N, V> > {
    std::atomic<T> values[N];

    template <typename... Args>
    AtomicVector(Args... args) {
        set(args...);
    }

    template <typename E>
    AtomicVector(const Expr<T, N, E>& e) {
        store(e);
    }

    template <typename E>
    AtomicVector& operator = (const Expr<T, N, E>& e) {
        store(e);
        return *this;
    }

    operator V () const {
        V v;
        for (int i = 0; i < N; i++) v[i] = values[i];
        return v;
    }

    template <typename... Args>
    void set(Args... args) {
        set_<0>(args...);
    }

    template <typename Op, typename E>
    void apply(const Expr<T, N, E>& e) {
        for (int i = 0; i < N; i++) atomic_apply<Op>(values[i], e[i]);
    }

    template <typename E>
    void store(const Expr<T, N, E>& e) {
        for (int i = 0; i < N; i++) values[i].store(e[i]);
    }

    T operator [] (int i) const {
        return values[i].load();
    }

    static ConstantExpr<T, N> zero() { return ConstantExpr<T, N>(T(0)); }
    static ConstantExpr<T, N> one() { return ConstantExpr<T, N>(T(1)); }

private:
    template <typename Op>
    static T atomic_apply(std::atomic<T>& a, T b) {
        T old_val = a.load();
        T desired_val = old_val + b;
        Op op;
        while(!a.compare_exchange_weak(old_val, desired_val))
            desired_val = op(old_val, b);
        return desired_val;
    }

    template <int I, typename... Args>
    void set_(T t, Args... args) {
        static_assert(I < N, "Too many initializers form atomic vector");
        values[I] = t;
        set_<I + 1>(args...);
    }

    template <int I>
    void set_() {}
};

} // namespace imba

#endif // IMBA_ATOMIC_VECTOR_H
