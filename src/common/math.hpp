#ifndef IMBA_MATH_HPP
#define IMBA_MATH_HPP

#include <cassert>
#include <cmath>

namespace imba {

static const float pi = 3.14159265359f;

inline float to_radians(float x) {
    return x * pi / 180.0f;
}

inline float to_degrees(float x) {
    return x * 180.0f / pi;
}

inline void assert_normalized(const Vec2& v) {
    assert(fabs(length(v) - 1.0f) < 0.0001f);
}

inline void assert_normalized(const Vec3& v) {
    assert(fabs(length(v) - 1.0f) < 0.0001f);
}

inline void assert_normalized(const Vec4& v) {
    assert(fabs(length(v) - 1.0f) < 0.0001f);
}

} // namespace imba

#endif // IMBA_MATH_HPP

