#ifndef IMBA_COMMON_H
#define IMBA_COMMON_H

#include <iostream>
#include <cstdlib>

namespace imba {

static const float pi = 3.14159265359f;

inline float radians(float x) {
    return x * pi / 180.0f;
}

inline float degrees(float x) {
    return x * 180.0f / pi;
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

template <typename T>
T sqr(T x) {
    return x * x;
}

template <typename T>
T rcp(T x) {
    return T(1) / x;
}

template <typename T>
T clamp(T a, T b, T c) {
    return (a < b) ? b : ((a > c) ? c : a);
}

template <typename T, typename U>
T lerp(T a, T b, U u) {
    return a * (1 - u) + b * u;
}

template <typename T, typename U>
T lerp(T a, T b, T c, U u, U v) {
    return a * (1 - u - v) + b * u + c * v;
}

template <typename T>
T reflect(T v, T n) {
    return v - (2 * dot(n, v)) * n;
}

#define assert_normalized(x) check_normalized(x, __FILE__, __LINE__)

template <typename T>
inline void check_normalized(const T& n, const char* file, int line) {
#ifdef CHECK_NORMALS
    const float len = length(n);
    const float tolerance = 0.001f;
    if (len < 1.0f - tolerance || len > 1.0f + tolerance) {
        std::cerr << "Vector not normalized in " << file << ", line " << line << std::endl;
        abort();
    }
#endif
}

#define V_ARRAY(T, N) static_cast<T*>(alloca((N) * sizeof(T)));

#define PARALLEL

#ifdef PARALLEL
#define TBB_PAR_FOR_BEGIN(BEG, END) tbb::parallel_for(tbb::blocked_range<int>(BEG, END), [&] (const tbb::blocked_range<int>& range) {
#define TBB_PAR_FOR_END })
#else
#define TBB_PAR_FOR_BEGIN(BEG, END) tbb::blocked_range<int> range(BEG, END);
#define TBB_PAR_FOR_END
#endif

} // namespace imba

#endif // IMBA_COMMON_H
