#ifndef IMBA_MATH_HPP
#define IMBA_MATH_HPP

#include <cmath>
#include <algorithm>

namespace imba {

static const float pi = 3.14159265359f;

inline float clamp(float x, float min, float max) {
    return std::min(std::max(x, min), max);
}

inline bool is_pow2(int i) {
    return ((i - 1) & i) == 0;
}

inline float to_radians(float x) {
    return x * pi / 180.0f;
}

inline float to_degrees(float x) {
    return x * 180.0f / pi;
}

} // namespace imba

#endif // IMBA_MATH_HPP

