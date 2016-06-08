#ifndef IMBA_RGB_H
#define IMBA_RGB_H

#include "vector.h"
#include "atomic_vector.h"

namespace imba {

template <typename T, int N>
struct Color : Vector<T, N> {
    Color() {}
    template <typename E>
    Color(const Expr<T, N, E>& e) : Vector<T, N>(e) {}
    template <typename... Args>
    explicit Color(Args... args) : Vector<T, N>(args...) {}
    explicit Color(T t) : Vector<T, N>(ConstantExpr<T, N>(t)) {}
};

/// Stores RGB color data
typedef Color<float, 3> rgb;
typedef Color<float, 4> rgba;

inline bool is_black(const rgb& a) {
    return a.x <= 0.0f && a.y <= 0.0f && a.z <= 0.0f;
}

inline bool is_black(const rgba& a) {
    return a.x <= 0.0f && a.y <= 0.0f && a.z <= 0.0f;
}

using atomic_rgb  = AtomicVector<float, 3, rgb>;
using atomic_rgba = AtomicVector<float, 4, rgba>;

} // namespace imba

#endif
