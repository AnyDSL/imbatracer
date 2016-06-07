#ifndef IMBA_ATOMIC_ARRAY_H
#define IMBA_ATOMIC_ARRAY_H

#include <atomic>

namespace imba {

template<typename T, int N>
class AtomicVector {
public:
    std::atomic<T> values[N];

    template <typename... Args>
    AtomicVector(Args... args) {
        set(args...);
    }

    template <typename V>
    AtomicVector(const V& v) {
        store(v);
    }

    AtomicVector() {}

    template <typename V>
    AtomicVector& operator= (const V& v) {
        store(v);
    }

    template <typename... Args>
    void set(Args... args) {
        set_<0>(args...);
    }

    template <typename Op, typename V>
    void apply(const V& v) {
        for (int i = 0; i < N; i++)
            atomic_apply<Op>(values[i], v[i]);
    }

    template <typename V>
    void store(const V& v) {
        for (int i = 0; i < N; i++)
            values[i].store(v[i]);
    }

    T operator [] (int i) const {
        return values[i].load();
    }

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

    template <int Idx, typename... Args>
    void set_(T t, Args... args) {
        values[Idx] = t;
        set_<Idx + 1>(args...);
    }

    void set_() {}
};

} // namespace imba

#endif