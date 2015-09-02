#ifndef IMBA_COMMON_H
#define IMBA_COMMON_H

namespace imba {

static const float pi = 3.14159265359f;

inline float radians(float x) {
    return x * pi / 180.0f;
}

inline float degrees(float x) {
    return x * 180.0f / pi;
}

template <typename T>
inline T clamp(T a, T b, T c) {
    return (a < b) ? b : (a > c) ? c : a;
}

inline int float_as_int(float f) {
    union { float vf; int vi; } v;
    v.vf = f;
    return v.vi;
}

inline float int_as_float(int i) {
    union { float vf; int vi; } v;
    v.vi = i;
    return v.vf;
}

} // namespace imba

#endif // IMBA_COMMON_H
