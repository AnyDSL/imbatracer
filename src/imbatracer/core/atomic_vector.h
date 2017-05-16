#ifndef IMBA_ATOMIC_VECTOR_H
#define IMBA_ATOMIC_VECTOR_H

#include <atomic>

#include "imbatracer/core/float4.h"

namespace imba {

template<typename T, int N, typename V>
struct AtomicVector {
    std::atomic<T> values[N];

    template <typename... Args>
    AtomicVector(Args... args) {
        set(args...);
    }

    AtomicVector(const V& e) {
        store(e);
    }

    AtomicVector& operator = (const V& e) {
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

    template <typename Op>
    void apply(const V& e) {
        for (int i = 0; i < N; i++) atomic_apply<Op>(values[i], e[i]);
    }

    void store(const V& e) {
        for (int i = 0; i < N; i++) values[i].store(e[i]);
    }

    T operator [] (int i) const {
        return values[i].load();
    }

    static V zero() { return V(T(0)); }
    static V one()  { return V(T(1)); }

private:
    template <typename Op>
    static T atomic_apply(std::atomic<T>& a, T b) {
        T old_val = a.load();
        Op op;
        T desired_val = op(old_val, b);
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
